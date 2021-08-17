#include <DeviceControl.h>

namespace hvk
{
	namespace control
	{
		class TestDevice : public Device
		{
		public:
			TestDevice();
			~TestDevice();

			virtual size_t write(Framebuffer& framebuffer) override;
		};
	}
}