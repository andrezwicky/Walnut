#pragma once

namespace Walnut
{
    enum class ImageFormat
    {
        None = 0,
        RGBA,
        RGBA32F,
        GBR3P
    };


	struct Scale
	{
		float x = 1;
		float y = 1;
	};
	struct ApplicationSpecification
	{
		std::string Name = "Walnut App";
		Scale ScaleDPI{};
		uint32_t Width = 1600;
		uint32_t Height = 900;
	};

	struct OffscreenImageSpec
	{
		std::string Name = "Offscreen Image";
		uint32_t Width = 3200;  // 1600;
		uint32_t Height = 1800; // 900;
	};
}