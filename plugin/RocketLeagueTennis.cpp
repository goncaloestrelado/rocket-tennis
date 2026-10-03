#include "pch.h"
#include "RocketLeagueTennis.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>

BAKKESMOD_PLUGIN(RocketLeagueTennis, "Rocket League Tennis (Phase 1: Spike E ball logging)", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

// [verify] Which event gives a usable per-tick callback. Engine.GameViewportClient.Tick is the usual
// choice in community plugins, but it is a render-frame tick, not necessarily the 120 Hz physics tick.
// The CSV logs dt_ms per row so the real rate can be read off the first run (see docs/spikes.md, spike E).
// Fallback candidate if the rate is wrong: "Function TAGame.Car_TA.SetVehicleInput" (per car, per physics tick).
static constexpr const char* kTickEvent = "Function Engine.GameViewportClient.Tick";

// A gap this long between ticks means pause/loading/replay-skip: don't pair samples across it.
static constexpr double kMaxPairGapMs = 250.0;

void RocketLeagueTennis::onLoad()
{
	_globalCvarManager = cvarManager;

	spikeEnabled = std::make_shared<bool>(true);
	echoTicks = std::make_shared<bool>(false);
	heightTol = std::make_shared<float>(12.0f);
	minVz = std::make_shared<float>(10.0f);

	cvarManager->registerCvar("rlt_spike_e_enabled", "1", "Spike E: log ball every tick and detect bounces", true, true, 0, true, 1).bindTo(spikeEnabled);
	cvarManager->registerCvar("rlt_spike_e_echo_ticks", "0", "Spike E: also print every tick row to the console (very spammy)", true, true, 0, true, 1).bindTo(echoTicks);
	cvarManager->registerCvar("rlt_spike_e_height_tol", "12", "Spike E: ball counts as near the floor when (z - radius) <= this many uu", true, true, 0, true, 200).bindTo(heightTol);
	cvarManager->registerCvar("rlt_spike_e_min_vz", "10", "Spike E: minimum |vertical velocity| in uu/s on both sides of a reversal", true, true, 0, true, 1000).bindTo(minVz);

	OpenCsv();

	gameWrapper->HookEventPost(kTickEvent, [this](std::string) { OnTick(); });

	LOG("[SpikeE] loaded, tick event = {}", kTickEvent);
}

void RocketLeagueTennis::onUnload()
{
	gameWrapper->UnhookEventPost(kTickEvent);
	if (csv.is_open())
	{
		csv.flush();
		csv.close();
	}
	LOG("[SpikeE] unloaded: {} ticks, {} bounces", tickCount, bounceCount);
}

void RocketLeagueTennis::OpenCsv()
{
	try
	{
		// [verify] GetDataFolder() resolves to %appdata%\bakkesmod\bakkesmod\data
		std::filesystem::path dir = gameWrapper->GetDataFolder() / "RocketLeagueTennis";
		std::filesystem::create_directories(dir);
		const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
		const std::string name = std::format("spike_e_{:%Y%m%d_%H%M%S}_utc.csv", now);
		csv.open(dir / name, std::ios::out | std::ios::trunc);
		if (csv.is_open())
		{
			csv << "wall_ms,dt_ms,tick,event,x,y,z,vx,vy,vz,side,height\n";
			LOG("[SpikeE] logging to {}", (dir / name).string());
		}
		else
		{
			LOG("[SpikeE] could not open {}", (dir / name).string());
		}
	}
	catch (const std::exception& e)
	{
		LOG("[SpikeE] CSV setup failed: {}", e.what());
	}
}

void RocketLeagueTennis::WriteRow(const char* event, unsigned long long tick, double wallMs, double dtMs, const BallSample& s)
{
	const float height = s.z - s.radius;
	if (csv.is_open())
	{
		csv << std::format("{:.1f},{:.2f},{},{},{:.2f},{:.2f},{:.2f},{:.2f},{:.2f},{:.2f},{},{:.2f}\n",
			wallMs, dtMs, tick, event, s.x, s.y, s.z, s.vx, s.vy, s.vz, s.side, height);
		// Bounce rows are rare and the interesting ones: flush immediately. Tick rows: batch.
		if (std::string_view(event) != "tick" || ++rowsSinceFlush >= 120)
		{
			csv.flush();
			rowsSinceFlush = 0;
		}
	}
	if (*echoTicks && std::string_view(event) == "tick")
	{
		LOG("[SpikeE] t={} pos=({:.1f},{:.1f},{:.1f}) vel=({:.1f},{:.1f},{:.1f}) side={}", tick, s.x, s.y, s.z, s.vx, s.vy, s.vz, s.side);
	}
}

void RocketLeagueTennis::OnTick()
{
	if (!*spikeEnabled)
	{
		havePrev = false;
		return;
	}

	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server)
	{
		havePrev = false;
		return;
	}
	BallWrapper ball = server.GetBall();
	if (!ball)
	{
		havePrev = false;
		return;
	}

	const auto now = std::chrono::steady_clock::now();
	const double dtMs = (lastTickTime.time_since_epoch().count() == 0)
		? 0.0
		: std::chrono::duration<double, std::milli>(now - lastTickTime).count();
	lastTickTime = now;
	const double wallMs = std::chrono::duration<double, std::milli>(now.time_since_epoch()).count();

	const Vector loc = ball.GetLocation();
	const Vector vel = ball.GetVelocity();

	BallSample cur;
	cur.x = loc.X; cur.y = loc.Y; cur.z = loc.Z;
	cur.vx = vel.X; cur.vy = vel.Y; cur.vz = vel.Z;
	cur.radius = ball.GetRadius();
	cur.side = SideOfY(cur.y);

	++tickCount;
	WriteRow("tick", tickCount, wallMs, dtMs, cur);

	// Bounce detection lives in BounceDetector.h (unit-tested). The lower of the two samples is checked for
	// "near the floor" because at 60-120 Hz a fast ball can already be several uu up on the first sample after
	// the reversal.
	if (havePrev)
	{
		BounceParams params;
		params.heightTol = *heightTol;
		params.minVz = *minVz;
		params.maxPairGapMs = kMaxPairGapMs;
		BallSample low;
		if (DetectBounce(prev, cur, dtMs, params, low))
		{
			++bounceCount;
			// Row carries the lower sample (best estimate of the contact point) so `side` is the half it hit.
			WriteRow("bounce", tickCount, wallMs, dtMs, low);
			LOG("[SpikeE] bounce #{} side={} pos=({:.1f},{:.1f},{:.1f}) vz {:.1f} -> {:.1f} height={:.1f} (side prev/cur: {}/{})",
				bounceCount, low.side, low.x, low.y, low.z, prev.vz, cur.vz, low.z - cur.radius, prev.side, cur.side);
		}
	}

	prev = cur;
	havePrev = true;
}
