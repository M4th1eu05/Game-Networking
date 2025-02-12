#include "stream.h"

Stream::Stream(uint32_t id, bool reliable)
    : streamID(id), reliable(reliable)
{
}

Stream::~Stream()
{
}

void Stream::SendData(std::span<const char> data)
{
}

void Stream::OnDataReceived(std::span<const char> data)
{
}