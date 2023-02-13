#pragma once

#include <Windows.h>
#include <TlHelp32.h>
#include <stdexcept>
#include <unordered_map>
#include <string> //switch to string_view later on


class Memory {
private:
	DWORD m_proc_id = 0; //used for the get_module_handle
	HANDLE m_proc_handle = NULL;

	struct module_info_t {
		std::uintptr_t module_base_address = 0;
		std::size_t module_size = 0;
	};

	std::unordered_map<std::wstring, module_info_t> m_proc_modules; //stores the name and in a pair (module base address, module size)

public:
	bool open_handle(const std::wstring& proc_name); //attach to process
	bool get_module_info(const std::wstring& mod_name); //stores the module info in a module_info_t struct inside the unordered map
	std::uintptr_t get_module_address(const std::wstring& mod_name) { return this->m_proc_modules[mod_name].module_base_address; }
	std::uintptr_t get_module_size(const std::wstring& mod_name) { return this->m_proc_modules[mod_name].module_size; }

	std::uintptr_t find_pattern(std::uintptr_t module_base_address, std::size_t module_size, const char* sig, const char* mask, int offset = 0); //to finish

	template <typename TYPE>
	TYPE read_memory(std::uintptr_t address) { //ReadProcessMemory wrapper
		if (!this->m_proc_handle)
			throw std::runtime_error("m_proc_handle has to be initialized with open_handle() in order to rpm the process");

		TYPE out = { 0 };
		if (!ReadProcessMemory(this->m_proc_handle, reinterpret_cast<LPCVOID>(address), &out, sizeof(TYPE), NULL))
			return NULL; //read failed

		return out;
	}

	template <typename TYPE>
	bool write_memory(std::uintptr_t address, TYPE& data) {
		if (!this->m_proc_handle)
			throw std::runtime_error("m_proc_handle has to be initialized with open_handle() in order to rpm the process");

		return WriteProcessMemory(this->m_proc_handle, reinterpret_cast<LPVOID>(address), data, sizeof(data), NULL);
	}

	template <typename TYPE>
	bool write_memory_with_size(std::uintptr_t address, TYPE& data, std::size_t size) {
		if (!this->m_proc_handle)
			throw std::runtime_error("m_proc_handle has to be initialized with open_handle() in order to rpm the process");

		return WriteProcessMemory(this->m_proc_handle, reinterpret_cast<LPVOID>(address), data, size, NULL);
	}

	std::uintptr_t virtual_alloc_ex(std::uintptr_t address, std::size_t size, DWORD alloc_type, DWORD protect) {
		if (!this->m_proc_handle)
			return NULL;

		return reinterpret_cast<std::uintptr_t>(VirtualAllocEx(this->m_proc_handle, reinterpret_cast<LPVOID>(address), size, alloc_type, protect));
	} //VirtualAllocEx wrapper

	~Memory() {
		if (this->m_proc_handle)
			CloseHandle(this->m_proc_handle);
	}
};