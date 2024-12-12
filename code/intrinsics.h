#include "math.h"

inline int32_t roundFloatToInt32(float num)
{
	return (int32_t)(num + 0.5f);
}
inline int32_t roundFloatToUInt32(float num)
{
	return (uint32_t)(num + 0.5f);
}

inline int32_t FloorFloatToInt32(float num)
{
	int32_t result = (int32_t)floorf(num);
	return result;
}
inline float Sin(float angle)
{
	float result = sinf(angle);
	return result;
}
inline float Tan(float angle)
{
	float result = tanf(angle);
	return result;
}
inline float Cos(float angle)
{
	float result = cosf(angle);
	return result;
}
inline float Atan2(float y, float x)
{
	float result = atan2f(y, x);
	return result;
}