#include "TestDevice.h"

#include <iostream>

namespace hvk
{
	namespace control
	{
		TestDevice::TestDevice()
		{

		}

		TestDevice::~TestDevice()
		{
			
		}

		size_t TestDevice::write(Framebuffer& framebuffer)
		{
			std::cout << "Writing framebuffer: " << framebuffer.size() << " colors" << std::endl;

			return framebuffer.size();
		}
	}
}