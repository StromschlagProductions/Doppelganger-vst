#include "TestSupport.h"

void captureOrder()
{
    for (double fs : { 44100., 48000., 192000. })
    {
        auto left = voice (fs, 4.25), right = left;
        for (auto& x : right) x *= -0.6f;
        dop::Learner learner; learner.prepare (fs); learner.beginCapture (2.0);
        for (size_t off = 0; off < left.size(); off += 317)
        {
            const int n = (int) std::min<size_t> (317, left.size() - off);
            learner.push (left.data() + off, n, nullptr, 2, 0, left.data() + off, right.data() + off);
        }
        auto capture = learner.takeCapturedChannels();
        CHECK (capture.stereo() && capture.size() == (size_t) (2 * fs));
        CHECK (std::equal (capture.left.begin(), capture.left.end(), left.end() - (long) capture.size()));
        CHECK (std::equal (capture.right.begin(), capture.right.end(), right.end() - (long) capture.size()));
        CHECK (learner.takeCapturedChannels().empty());

        // Changing layout after the ring is full must also downmix the existing slots safely.
        learner.reset(); learner.beginCapture (0.01);
        learner.push (left.data(), 3000, nullptr, 2, 0, left.data(), right.data());
        learner.push (left.data(), 3000);
        capture = learner.takeCapturedChannels();
        CHECK (! capture.stereo() && capture.size() == (size_t) (0.01 * fs));
    }
}

void sameGainReference()
{
    for (double fs : { 44100., 48000., 192000. })
    {
        auto input = voice (fs, 28.0);
        // The retained tail is much quieter than the full learned passage. The old calibrator normalised
        // that tail independently and certified -23 LUFS even though the live gain produced about -40 LUFS.
        for (size_t i = (size_t) (12 * fs); i < input.size(); ++i) input[i] *= 0.12f;
        dop::Learner learner; learner.prepare (fs); learner.beginCapture (fs > 48000 ? 40.0 : 4.0);
        learner.push (input.data(), (int) input.size());
        const auto profile = learner.finish();
        const auto capture = learner.takeCapturedChannels();
        CHECK (profile.valid && capture.size() <= dop::Learner::kMaxCaptureSamples);
        dop::EngineParams p; p.lowcutHz = 20; p.deessOn = false; p.leveler = 0;
        p.takeLufs = profile.loudnessLufs;
        const auto bank = dop::flatBank().coeffs;
        const auto result = dop::autopilot::calibrate (capture, fs, p, bank, 0, 20);
        CHECK (result.ok);
        p.calibDb = result.calibDb; p.peakGuard = true;
        dop::Engine live; live.prepare (fs); live.setParams (p); live.setBank (bank); live.reset();
        auto actual = capture.left;
        float* ptr[] = { actual.data() }; live.process (ptr, 1, (int) actual.size(), nullptr);
        const float measured = dop::autopilot::integratedLoudness (actual, fs);
        CHECK (std::abs (measured - result.outLufs) <= 0.06f);
        CHECK (std::abs (result.outLufs - p.targetLufs) > 1.0f); // correction limit is reported honestly
    }
}

void leadingSilence()
{
    constexpr double fs = 48000;
    auto input = std::vector<float> ((size_t) (40 * fs), 0.0f);
    auto speech = voice (fs, 6.0); input.insert (input.end(), speech.begin(), speech.end());
    dop::Learner learner; learner.prepare (fs); learner.beginCapture (40);
    learner.push (input.data(), (int) input.size());
    const auto profile = learner.finish(); const auto capture = learner.takeCapturedChannels();
    CHECK (profile.valid && profile.speechSeconds >= 4);
    CHECK (std::equal (capture.left.end() - (long) speech.size(), capture.left.end(), speech.begin()));
    dop::EngineParams p; p.lowcutHz = 20; p.takeLufs = profile.loudnessLufs;
    const auto result = dop::autopilot::calibrate (capture, fs, p, dop::flatBank().coeffs, 0, 20);
    CHECK (result.ok);
}

int main()
{
    captureOrder(); sameGainReference(); leadingSilence();
    std::puts ("Bounded capture, layout changes and live/calibration gain agreement passed");
}
