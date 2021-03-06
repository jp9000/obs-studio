extern "C" EXPORT void winrt_initialize()
{
	winrt::init_apartment(winrt::apartment_type::multi_threaded);
}

extern "C" EXPORT void winrt_uninitialize()
{
	winrt::uninit_apartment();
}

static winrt::Windows::System::DispatcherQueueController
CreateDispatcherQueueController()
{
	DispatcherQueueOptions options{sizeof(DispatcherQueueOptions),
				       DQTYPE_THREAD_CURRENT, DQTAT_COM_STA};

	winrt::Windows::System::DispatcherQueueController controller{nullptr};
	winrt::check_hresult(CreateDispatcherQueueController(
		options,
		reinterpret_cast<
			ABI::Windows::System::IDispatcherQueueController **>(
			winrt::put_abi(controller))));
	return controller;
}

struct winrt_disaptcher {
	winrt::Windows::System::DispatcherQueueController controller{nullptr};
};

extern "C" EXPORT struct winrt_disaptcher *winrt_dispatcher_init()
{
	struct winrt_disaptcher *dispatcher = NULL;
	try {
		if (winrt::Windows::Foundation::Metadata::ApiInformation::
			    IsApiContractPresent(
				    L"Windows.Foundation.UniversalApiContract",
				    5)) {
			winrt::Windows::System::DispatcherQueueController
				controller = CreateDispatcherQueueController();

			dispatcher = new struct winrt_disaptcher;
			dispatcher->controller = std::move(controller);
		}
	} catch (const winrt::hresult_error &err) {
		blog(LOG_ERROR, "winrt_dispatcher_init (0x%08X): %ls",
		     err.to_abi(), err.message().c_str());
	} catch (...) {
		blog(LOG_ERROR, "winrt_dispatcher_init (0x%08X)",
		     winrt::to_hresult());
	}

	return dispatcher;
}

extern "C" EXPORT void
winrt_dispatcher_free(struct winrt_disaptcher *dispatcher)
try {
	delete dispatcher;
} catch (const winrt::hresult_error &err) {
	blog(LOG_ERROR, "winrt_dispatcher_free (0x%08X): %ls", err.to_abi(),
	     err.message().c_str());
} catch (...) {
	blog(LOG_ERROR, "winrt_dispatcher_free (0x%08X)", winrt::to_hresult());
}
