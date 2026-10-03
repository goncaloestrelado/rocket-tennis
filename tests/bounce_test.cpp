// Offline test for the Spike E bounce detector. No game, no SDK.
#include "BounceDetector.h"

#include <cstdio>
#include <vector>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s (line %d)\n", msg, __LINE__); ++failures; } } while (0)

static const float R = 91.25f;     // approx. ball radius, uu
static const float G = -650.0f;    // uu/s^2

// Simulates a ball with restitution `e` and returns the number of detected bounces.
// hz = tick rate; startZ = center height; y = lateral position (sets side).
static int SimulateDrops(float hz, float startZ, float y, float e, int expectedFloorHits, float* lastContactY = nullptr, int* lastSide = nullptr)
{
	const double dt = 1.0 / hz;
	BallSample cur; cur.z = startZ; cur.y = y; cur.radius = R; cur.side = SideOfY(y);
	BallSample prev = cur;
	BounceParams p;
	int detected = 0, floorHits = 0;
	for (int i = 0; i < 20000 && floorHits < expectedFloorHits; ++i)
	{
		prev = cur;
		cur.vz += (float)(G * dt);
		cur.z += (float)(cur.vz * dt);
		if (cur.z < R && cur.vz < 0) { cur.z = R; cur.vz = -cur.vz * e; ++floorHits; }
		cur.side = SideOfY(cur.y);
		if (i > 0)
		{
			BallSample contact;
			if (DetectBounce(prev, cur, dt * 1000.0, p, contact))
			{
				++detected;
				if (lastContactY) *lastContactY = contact.y;
				if (lastSide) *lastSide = contact.side;
			}
		}
	}
	return detected;
}

int main()
{
	// 1. Every floor hit of a dropped ball is detected exactly once, at 60 and 120 Hz.
	for (float hz : {60.0f, 120.0f})
		CHECK(SimulateDrops(hz, 800.0f, 1000.0f, 0.6f, 4) >= 3, "drop detected at each bounce");

	// 2. Detection count is exactly the number of floor hits (no doubles, none missed) at 120 Hz.
	{
		const int d = SimulateDrops(120.0f, 800.0f, 1000.0f, 0.6f, 3);
		CHECK(d == 3, "exactly 3 detections for 3 floor hits");
	}

	// 3. Side comes from the contact sample.
	{
		int side = 99; float cy = 0;
		SimulateDrops(120.0f, 800.0f, -1500.0f, 0.6f, 1, &cy, &side);
		CHECK(side == -1, "ball dropped at Y<0 reports side -1");
		SimulateDrops(120.0f, 800.0f, 1500.0f, 0.6f, 1, &cy, &side);
		CHECK(side == 1, "ball dropped at Y>0 reports side +1");
		CHECK(SideOfY(0.0f) == 0, "Y == 0 is side 0");
	}

	BounceParams p;
	BallSample c;

	// 4. Apex of a free-flight arc (vz + -> -) is not a bounce.
	{
		BallSample a; a.z = 600; a.vz = 5; a.radius = R;
		BallSample b = a; b.vz = -5; b.z = 600;
		CHECK(!DetectBounce(a, b, 8.3, p, c), "apex is not a bounce");
	}

	// 5. down->up reversal high in the air (e.g. a car hit) is rejected by the height check.
	{
		BallSample a; a.z = 700; a.vz = -400; a.radius = R;
		BallSample b = a; b.vz = 900; b.z = 695;
		CHECK(!DetectBounce(a, b, 8.3, p, c), "mid-air reversal is not a bounce");
	}

	// 6. Tiny reversals (rolling jitter) are ignored.
	{
		BallSample a; a.z = R + 0.5f; a.vz = -2; a.radius = R;
		BallSample b = a; b.vz = 2;
		CHECK(!DetectBounce(a, b, 8.3, p, c), "jitter below min_vz is not a bounce");
	}

	// 7. A pause (big gap between ticks) is never paired.
	{
		BallSample a; a.z = R + 1; a.vz = -300; a.radius = R;
		BallSample b = a; b.vz = 200;
		CHECK(!DetectBounce(a, b, 5000.0, p, c), "samples across a long gap are not paired");
	}

	// 8. Fast bounce at 60 Hz: second sample already well above floor, first is near it -> still detected.
	{
		BallSample a; a.z = R + 6; a.vz = -2000; a.radius = R;
		BallSample b = a; b.z = R + 25; b.vz = 1500;
		CHECK(DetectBounce(a, b, 16.7, p, c), "fast bounce detected via the lower sample");
		CHECK(c.z == a.z, "contact estimate is the lower sample");
	}

	// 9. Ball that only rises or only falls is never a bounce.
	{
		BallSample a; a.z = R + 2; a.vz = 300; a.radius = R;
		BallSample b = a; b.vz = 250;
		CHECK(!DetectBounce(a, b, 8.3, p, c), "rising only");
		a.vz = -300; b.vz = -350;
		CHECK(!DetectBounce(a, b, 8.3, p, c), "falling only");
	}

	if (failures == 0) std::printf("bounce_test: all checks passed\n");
	return failures == 0 ? 0 : 1;
}
