STRUCT_BEGIN(wsmp)
FIELD(std::uint32_t, cbSize)
FIELD(std::uint16_t, usUnityNote)
FIELD(std::int16_t, sFineTune)
FIELD(std::int32_t, lGain)
FIELD(std::uint32_t, fulOptions)
FIELD(std::uint32_t, cSampleLoops)
VARARR_OFF(wsmp_loop, loops, cSampleLoops, cbSize)
STRUCT_END(wsmp)