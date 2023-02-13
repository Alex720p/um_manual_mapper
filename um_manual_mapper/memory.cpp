#include "./memory.hpp"

bool Memory::open_handle(const std::wstring& proc_name) {
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 proc_entry;
	proc_entry.dwSize = sizeof(PROCESSENTRY32);
	if (Process32First(snapshot, &proc_entry)) {
		while (Process32Next(snapshot, &proc_entry)) {
			if (!proc_name.compare(proc_entry.szExeFile)) { //in this case == 0 means that the strings are 'equal'
				this->m_proc_id = proc_entry.th32ProcessID;
				this->m_proc_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, proc_entry.th32ProcessID);
				break;
			}
		}
	}

	CloseHandle(snapshot);

	if (this->m_proc_handle == NULL)
		return false;

	return true; //handle opened to desired process
}

bool Memory::get_module_info(const std::wstring& mod_name) {
	if (!this->m_proc_id)
		throw std::runtime_error("m_proc_id has to be initialized in order to get the module of the process");

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, m_proc_id);
	MODULEENTRY32 mod_entry;
	mod_entry.dwSize = sizeof(MODULEENTRY32);
	if (Module32First(snapshot, &mod_entry)) {
		while (Module32Next(snapshot, &mod_entry)) {
			if (!mod_name.compare(mod_entry.szModule)) { //in this case == 0 means that the strings are 'equal'
				Memory::module_info_t module_info = { reinterpret_cast<std::uintptr_t>(mod_entry.modBaseAddr), mod_entry.modBaseSize };
				this->m_proc_modules[mod_name] = module_info;
				CloseHandle(snapshot);
				return true;
			}
		}
	}
	CloseHandle(snapshot);
	return false; //module not found
}

std::uintptr_t Memory::find_pattern(std::uintptr_t module_base_address, std::size_t module_size, const char* sig, const char* mask, int offset) {
	MEMORY_BASIC_INFORMATION mem_basic_info = { 0 };
	VirtualQueryEx(this->m_proc_handle, reinterpret_cast<LPCVOID>(module_base_address), &mem_basic_info, sizeof(mem_basic_info)); //getting the regions size in mem_basic_info

	char* page_buffer = new char[mem_basic_info.RegionSize + strlen(mask)]; //structure: old page content (size of mask) for sig across pages and new page content (size of region)
	for (std::size_t i = 0; i < module_size; i += mem_basic_info.RegionSize) { //going throught the regions

		DWORD old_protect;
		VirtualProtectEx(this->m_proc_handle, reinterpret_cast<LPVOID>(module_base_address + i), mem_basic_info.RegionSize, PAGE_READONLY, &old_protect);

		if (!ReadProcessMemory(this->m_proc_handle, reinterpret_cast<LPCVOID>(module_base_address + i), page_buffer + strlen(mask), mem_basic_info.RegionSize, NULL))
			throw std::runtime_error("ReadProcessMemory failed in find_pattern(), make sure the params are correct");

		VirtualProtectEx(this->m_proc_handle, reinterpret_cast<LPVOID>(module_base_address + i), mem_basic_info.RegionSize, old_protect, &old_protect);

		for (std::size_t j = 0; j < mem_basic_info.RegionSize; j++) {
			for (std::size_t k = 0; k < strlen(mask); k++) {
				if (page_buffer[j + k] != sig[k] && mask[k] != '?')
					break; //sequence not matching sig, break and go next

				if (k == strlen(mask) - 1) {
					delete[] page_buffer;
					return i + j - strlen(mask) + offset; // sig found !
				}

			}
		}
		memcpy(page_buffer, page_buffer + mem_basic_info.RegionSize, strlen(mask)); //copy the end of the page for across pages sig
	}

	delete[] page_buffer;
	return 0;
}