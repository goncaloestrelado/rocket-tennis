#pragma once
// SDK-free so it can be unit-tested off-game (tests/bounce_test.cpp, run in CI).

// One sample of the ball taken on a tick.
struct BallSample
{
	float x = 0, y = 0, z = 0;
	float vx = 0, vy = 0, vz = 0;
	float radius = 0;
	int side = 0; // sign of ball Y (CLAUDE.md): +1, -1, or 0 exactly on the net line
};

inline int SideOfY(float y)
{
	return y > 0.0f ? 1 : (y < 0.0f ? -1 : 0);
}

struct BounceParams
{
	float heightTol = 12.0f;     // uu: (z - radius) of the lower sample must be <= this
	float minVz = 10.0f;         // uu/s: |vz| must exceed this on both sides of the reversal
	double maxPairGapMs = 250.0; // samples further apart than this are not paired (pause/loading/replay)
};

// A bounce is: vz goes from clearly-downward (prev) to clearly-upward (cur), and the lower of the two
// samples is near the floor. The lower sample is the best estimate of the contact point; the caller reads
// its side from `contact`. Ball hit upward by a car close to the floor also matches (see docs/spikes.md).
inline bool DetectBounce(const BallSample& prev, const BallSample& cur, double dtMs, const BounceParams& p, BallSample& contact)
{
	if (dtMs > p.maxPairGapMs) return false;
	if (!(prev.vz < -p.minVz)) return false;
	if (!(cur.vz > p.minVz)) return false;
	const BallSample& low = (prev.z <= cur.z) ? prev : cur;
	if (low.z - cur.radius > p.heightTol) return false;
	contact = low;
	return true;
}
