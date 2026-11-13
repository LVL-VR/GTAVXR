#pragma once
// UPDATE-only value gate; no pointers, locks, queries or damage authority.
namespace gx_armed_phase
{
inline bool handEnabled(unsigned hand, bool physicalMode, bool legacyWitness, bool extended)
{
    return hand < 2 && (physicalMode || (legacyWitness && hand == 1 && !extended));
}
class UpdateOnce
{
    unsigned epoch_[2], frame_[2];

  public:
    UpdateOnce()
    {
        boundary();
    }
    void boundary()
    {
        for (unsigned h = 0; h < 2; ++h)
        {
            epoch_[h] = 0;
            frame_[h] = 0;
        }
    }
    bool begin(unsigned hand, unsigned epoch, unsigned nativeFrame)
    {
        if (hand >= 2 || !epoch || !nativeFrame)
            return false;
        if (epoch_[hand] == epoch && nativeFrame <= frame_[hand])
            return false;
        epoch_[hand] = epoch;
        frame_[hand] = nativeFrame;
        return true;
    }
};
}
