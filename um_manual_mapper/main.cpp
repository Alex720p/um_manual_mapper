#include <iostream>
#include "memory.hpp"


#include <fstream>
#include <Windows.h>
#include <winnt.h>


/*
* Sources:
* https://www.joachim-bauch.de/tutorials/loading-a-dll-from-memory/
* https://0xrick.github.io/win-internals
* https://research32.blogspot.com/2015/01/base-relocation-table.html
*/



//clone and fix the original memory class

void main() {

	std::ifstream file("message_box.dll", std::ios_base::binary);

	if (!file.is_open())
		return;

	Memory memory;

	if (!memory.open_handle(L"ac_client.exe"))
		return;


	file.seekg(0, file.end);
	int file_size = file.tellg();
	file.seekg(0, file.beg);

	char* buffer = new char[file_size];
	file.read(buffer, file_size);


	file.close();

	IMAGE_DOS_HEADER* dos_header = reinterpret_cast<IMAGE_DOS_HEADER*>(buffer); 
	IMAGE_NT_HEADERS* nt_header = reinterpret_cast<IMAGE_NT_HEADERS*>(buffer + dos_header->e_lfanew);

	std::uintptr_t base = memory.virtual_alloc_ex(NULL, nt_header->OptionalHeader.SizeOfImage, MEM_RESERVE, PAGE_READWRITE); //we'll just let virtualalloc choose the address and rebase after
	if (!base)		
		return;


	IMAGE_SECTION_HEADER* section_header = IMAGE_FIRST_SECTION(nt_header);
	
	std::cout << "dll base: " << base << std::endl;

	//loading the dll into target process memory
	for (std::size_t i = 0; i < nt_header->FileHeader.NumberOfSections; i++) {
		std::uintptr_t dest = 0; //address where we'll copy the section content
		std::size_t size = section_header->SizeOfRawData; //used for count in memcpy
		if (section_header->SizeOfRawData == 0) { //TODO: to be checked
			if (section_header->Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA) {
				size = nt_header->OptionalHeader.SizeOfInitializedData;
				dest = memory.virtual_alloc_ex(base + section_header->VirtualAddress, size, MEM_COMMIT, PAGE_READWRITE);
			}
			else if (section_header->Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA) {
				size = nt_header->OptionalHeader.SizeOfUninitializedData;
				dest = memory.virtual_alloc_ex(base + section_header->VirtualAddress, size, MEM_COMMIT, PAGE_READWRITE);
			}
		}
		else
			dest = memory.virtual_alloc_ex(base + section_header->VirtualAddress, section_header->SizeOfRawData, MEM_COMMIT, PAGE_READWRITE);

		//copying the section into the target memory
		char* section = new char[size];
		std::memcpy(section, buffer + section_header->PointerToRawData, size);
		memory.write_memory_with_size(dest, section, size);
		delete[] section;

		section_header++; //go to next section_header
	}

	//base relocation
	IMAGE_DATA_DIRECTORY base_reloc_dir = nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
	int image_delta = base - nt_header->OptionalHeader.ImageBase; //int since it could be nagative
	IMAGE_BASE_RELOCATION* base_reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(base_reloc_dir.VirtualAddress);

	//TODO: rewrite this part to not rely on ReadProcessMemory
	for (std::size_t i = 0; i <= base_reloc_dir.Size; i += base_reloc->SizeOfBlock) { //going over the reloc blocks
		IMAGE_BASE_RELOCATION* base_reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(base_reloc_dir.VirtualAddress += i);
		std::size_t num_of_entries = (base_reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / 2; //num of entries (of 2 bytes) in the reloc block
		for (std::size_t i = sizeof(IMAGE_BASE_RELOCATION); i <= base_reloc->SizeOfBlock; i += 2) { //going over all the entries
			if ((*(reinterpret_cast<BYTE*>(base_reloc)+ i + 1) & (IMAGE_REL_BASED_HIGHLOW << 4)) == (IMAGE_REL_BASED_HIGHLOW << 4)) { //prob a way cleaner way to check but it *should* work
				//cast to a word and then check bytes (will not be reversed) + read the endian wikipedia page https://en.wikipedia.org/wiki/Endianness
																																	  
				//fix
			}
		}

	}

	//copy the relocated stuff to target memory, optimize the stuff before since we're copying twice the .reloc section


	//.reloc section


	delete[] buffer;
}

//release with VirtualFreeEx when exit