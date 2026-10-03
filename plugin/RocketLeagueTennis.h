#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include <chrono>
#include <fstream>

#include "BounceDetector.h"

#include "version.h"
constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

class RocketLeagueTennis : public BakkesMod::Plugin::BakkesModPlugin
{
	// --- Spike E: tick logging + bounce detection (Phase 1) ---
	std::shared_ptr<bool> spikeEnabled;     // rlt_spike_e_enabled
	std::shared_ptr<bool> echoTicks;        // rlt_spike_e_echo_ticks (also print every tick row to the console)
	std::shared_ptr<float> heightTol;       // rlt_spike_e_height_tol (uu above ball radius that still counts as "near the floor")
	std::shared_ptr<float> minVz;           // rlt_spike_e_min_vz (uu/s, ignores reversals smaller than this)

	std::ofstream csv;
	std::chrono::steady_clock::time_point lastTickTime{};
	bool havePrev = false;
	BallSample prev;
	unsigned long long tickCount = 0;
	unsigned long long bounceCount = 0;
	unsigned int rowsSinceFlush = 0;

	void onLoad() override;
	void onUnload() override;

	void OnTick();
	void OpenCsv();
	void WriteRow(const char* event, unsigned long long tick, double wallMs, double dtMs, const BallSample& s);
};
