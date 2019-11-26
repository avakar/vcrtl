#include <ntddk.h>

extern "C" DRIVER_INITIALIZE DriverEntry;

extern "C" NTSTATUS DriverEntry(
	PDRIVER_OBJECT DriverObject,
	PUNICODE_STRING RegistryPath
)
{
	try
	{
		throw 1;
	}
	catch (int)
	{
	}

	(void)DriverObject;
	(void)RegistryPath;
	return STATUS_SUCCESS;
}
