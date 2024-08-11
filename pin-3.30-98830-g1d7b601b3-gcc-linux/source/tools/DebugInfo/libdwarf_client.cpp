/*
 * Copyright (C) 2022-2022 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*
 * This pintool is testing the libdwarf library and is also provided as an example for how to use the libdwarf library.
 * The libdwarf library is provided as part of the Pin kit for pintool developers to use.
 * Pintool mode of operation:
 * The pintool does not actually use any instrumentations and does not run the application.
 * It runs its logic from the main() function and uses the API of libdwarf.so directly.
 * The list of subprograms read from the DWARF information is dumped to a file in a specific format
 * that needs to match the format in the reference file since the files will be compared.
 * The reason for using a pintool for this purpose is because it is already linked with Pin CRT and is simple to use.
 * The pintool itself will only fail in case there is an error in retrieving the data.
 * 
 */

#include "pin.H"
#include <iostream>
#include <fstream>
using std::cerr;
using std::endl;
using std::string;

#include "dwarf.h"
#include "libdwarf.h"

// The input file - the binary for which we are extracting the dwarf data
KNOB< string > KnobBinary(KNOB_MODE_WRITEONCE, "pintool", "bin", "", "specify binary file name for dwarf parsing");
// The output file - where to dump the subroutines list
KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "subprograms.out", "specify stdout file name");

static std::ofstream outfile;

static const int maxRecursionLevel = 100;

// According the documentation of libdWARF, isInfo should be TRUE for reading through .debug_info
// and FALSE for reading through DWARF4 .debug_types. Experiments showed the result is actually the same in this test.
static const Dwarf_Bool isInfo = TRUE;

INT32 Usage()
{
    std::cerr << "This tool parses the DWARF of the given file." << std::endl;
    std::cerr << KNOB_BASE::StringKnobSummary() << std::endl;
    return -1;
}

/*
 * Return TRUE if the retval passed is an error, and print the error. Return FALSE otherwise.
 */
static BOOL IsError(const char* call_name, int retval, Dwarf_Error& error, const Dwarf_Debug& dbg)
{
    if (retval == DW_DLV_ERROR)
    {
        std::cerr << call_name << "() failed ";
        Dwarf_Unsigned dw_errno = dwarf_errno(error);
        char* dw_errmsg         = dwarf_errmsg(error);
        std::cerr << "; errno " << std::dec << dw_errno << " (" << dw_errmsg << ")" << std::endl;
        dwarf_dealloc_error(dbg, error);
        error = NULL;
        return TRUE;
    }
    return FALSE;
}

/*
 * Move to another node in the DWARF tree through one of the node's attributes.
 * In some cases an attribute is a link to another node in the tree.
 * For example, DW_AT_specification.
 * This function will follow the link and return the linked die.
 * In case of failure the function will return NULL.
 */
static Dwarf_Die GetLinkedDie(int attributeId, Dwarf_Die die, const Dwarf_Debug& dbg)
{
    Dwarf_Error error;
    Dwarf_Die ret_die       = NULL;
    Dwarf_Attribute attrib  = 0;
    Dwarf_Off attrib_offset = 0;

    int res = dwarf_attr(die, attributeId, &attrib, &error);
    IsError("dwarf_attr", res, error, dbg);
    if (res == DW_DLV_OK)
    {
        res = dwarf_global_formref(attrib, &attrib_offset, &error);
        IsError("dwarf_global_formref", res, error, dbg);
        if (res == DW_DLV_OK)
        {
            res = dwarf_offdie_b(dbg, attrib_offset, isInfo, &ret_die, &error);
            IsError("dwarf_offdie_b", res, error, dbg);
        }
        dwarf_dealloc(dbg, attrib, DW_DLA_ATTR);
    }
    return ret_die;
}

/*
 * Read the name (short or mangled) of a subprogram from a DWARF node.
 * In some cases the name (DW_AT_name/DW_AT_linkage_name/DW_AT_MIPS_linkage_name)
 * will appear as part of the original die, and in other cases we will need to
 * travel through additional nodes to get the name. 
 * The function returns TRUE always because it is possible for a valid node not to have a mangled name for example.
 * However if a name is not found then the name argument will get a NULL value.
 */
static BOOL GetSubprogramName(int attributeId, char** name, Dwarf_Die die, const Dwarf_Debug& dbg)
{
    Dwarf_Error error;
    Dwarf_Die curr     = die;
    const int maxSteps = 3;
    *name              = NULL;

    for (int i = 0; i < maxSteps; i++)
    {
        int res = dwarf_die_text(curr, attributeId, name, &error);
        IsError("dwarf_die_text", res, error, dbg);
        if (res == DW_DLV_OK)
        {
            return TRUE;
        }
        Dwarf_Die next = GetLinkedDie(DW_AT_abstract_origin, curr, dbg);
        if (next == NULL)
        {
            next = GetLinkedDie(DW_AT_specification, curr, dbg);
        }
        if (next == NULL) break;
        curr = next;
    }
    return TRUE;
}

/*
 * Print subprograms in a format expected by the comparison script.
 */
static BOOL PrintDie(Dwarf_Die die, const Dwarf_Debug& dbg)
{
    Dwarf_Error error;
    Dwarf_Half tag = 0;
    int res;

    // Tag
    res = dwarf_tag(die, &tag, &error);
    if (IsError("dwarf_tag", res, error, dbg)) return FALSE;
    if (res == DW_DLV_NO_ENTRY)
    {
        std::cerr << "Dwarf_Die doesn't have a TAG value" << std::endl;
        return FALSE;
    }
    ASSERTX(res == DW_DLV_OK);

    // Skip all tags except DW_TAG_subprogram and DW_TAG_inlined_subroutine
    BOOL inlined = FALSE;
    switch (tag)
    {
        case DW_TAG_subprogram:
            break;

        case DW_TAG_inlined_subroutine:
            inlined = TRUE;
            break;

        default:
            return TRUE;
    }

    // Name (DW_AT_name)
    char* shortName = NULL;
    GetSubprogramName(DW_AT_name, &shortName, die, dbg);

    // Mangled name (DW_AT_linkage_name / DW_AT_MIPS_linkage_name)
    char* mangledName = NULL;
    if (!GetSubprogramName(DW_AT_linkage_name, &mangledName, die, dbg))
    {
        GetSubprogramName(DW_AT_MIPS_linkage_name, &mangledName, die, dbg);
    }
    if ((shortName == NULL) && (mangledName == NULL))
    {
        return TRUE;
    }
    if (mangledName == NULL)
    {
        mangledName = shortName;
    }

    // low PC
    Dwarf_Addr lowPC = 0;
    res              = dwarf_lowpc(die, &lowPC, &error);
    if (IsError("dwarf_lowpc", res, error, dbg)) return FALSE;

    // High PC
    Dwarf_Addr highPC               = 0;
    Dwarf_Half form                 = 0;
    enum Dwarf_Form_Class formClass = DW_FORM_CLASS_UNKNOWN;
    res                             = dwarf_highpc_b(die,        // dw_die
                         &highPC,    // dw_return_addr
                         &form,      // dw_return_form
                         &formClass, // dw_return_class
                         &error      // dw_error
    );
    if (IsError("dwarf_highpc_b", res, error, dbg)) return FALSE;

    switch (formClass)
    {
        case DW_FORM_CLASS_CONSTANT: // The value is an offset
            highPC = lowPC + highPC - 1;
            break;

        case DW_FORM_CLASS_ADDRESS: // The value is the address past the last instruction
            highPC--;
            break;

        case DW_FORM_CLASS_UNKNOWN:
            break;

        default:
            ASSERT(FALSE, "Illegal form class " + hexstr(formClass) + "\n");
    }

    if (inlined | (lowPC && highPC))
    {
        outfile << std::hex << lowPC << " | " << highPC << " | " << mangledName << " | " << shortName << " | "
                << (inlined ? "inline=YES" : "inline=NO") << std::endl;
    }
    return TRUE;
}

/*
 * Traverse the tree and print its nodes.
 * The function will print the node passed as root, and then call this function for each of its children.
 */
static BOOL TraverseDwarfTree(Dwarf_Die root, const Dwarf_Debug& dbg, int& recursionLevel)
{
    if (recursionLevel > maxRecursionLevel) // This is just a precaution in case the DWARF tree is invalid or has loops
    {
        std::cerr << "Recursion level exceeds " << std::dec << maxRecursionLevel << " DWARF may be invalid" << std::endl;
        return TRUE;
    }

    Dwarf_Error error;
    std::vector< Dwarf_Die > children;
    int res;

    // Print current node
    if (!PrintDie(root, dbg))
    {
        return FALSE;
    }

    // Create list of children for current node
    Dwarf_Die child = NULL;
    res             = dwarf_child(root, &child, &error);
    if (IsError("dwarf_child", res, error, dbg)) return FALSE;
    if (res == DW_DLV_OK)
    {
        children.push_back(child);

        Dwarf_Die curr    = child;
        Dwarf_Die sibling = NULL;
        while (TRUE)
        {
            res = dwarf_siblingof_b(dbg, curr, isInfo, &sibling, &error);
            if (IsError("dwarf_siblingof_b", res, error, dbg)) return FALSE;
            if (res == DW_DLV_NO_ENTRY)
            {
                break;
            }
            ASSERTX(res == DW_DLV_OK);
            children.push_back(sibling);
            curr = sibling;
        }
    }
    // Release current node
    dwarf_dealloc(dbg, root, DW_DLA_DIE);

    // Iterate on the list of children and recurse into this function with child as root
    recursionLevel++;
    for (size_t i = 0; i < children.size(); i++)
    {
        if (!TraverseDwarfTree(children[i], dbg, recursionLevel))
        {
            return FALSE;
        }
    }
    recursionLevel--;
    return TRUE;
}

/*
 * Iterate on the compilation units and for each one print the subprograms in it.
 */
static BOOL IterateOnCompilationUnits(const Dwarf_Debug& dbg)
{
    while (TRUE)
    {
        Dwarf_Unsigned cuHeaderLen;
        Dwarf_Half versionStamp;
        Dwarf_Off abbrevOffset;
        Dwarf_Half addressSize;
        Dwarf_Half lengthSize;
        Dwarf_Half extensionSize;
        Dwarf_Sig8 typeSignature;
        Dwarf_Unsigned typeOffset;
        Dwarf_Unsigned nextCuHeaderOffset;
        Dwarf_Half headerCuType;
        Dwarf_Error error;
        int res;

        res = dwarf_next_cu_header_d(dbg,                 // dw_dbg
                                     isInfo,              // dw_is_info
                                     &cuHeaderLen,        // dw_cu_header_length
                                     &versionStamp,       // dw_version_stamp
                                     &abbrevOffset,       // dw_abbrev_offset
                                     &addressSize,        // dw_address_size
                                     &lengthSize,         // dw_length_size
                                     &extensionSize,      // dw_extension_size
                                     &typeSignature,      // dw_type_signature
                                     &typeOffset,         // dw_typeoffset
                                     &nextCuHeaderOffset, // dw_next_cu_header_offset
                                     &headerCuType,       // dw_header_cu_type
                                     &error               // dw_error
        );
        if (IsError("dwarf_next_cu_header_d", res, error, dbg)) return FALSE;
        if (res == DW_DLV_NO_ENTRY) // no more compilation units, we're done
        {
            return TRUE;
        }
        ASSERTX(res == DW_DLV_OK);

        Dwarf_Die die = NULL;
        res           = dwarf_siblingof_b(dbg,    // dw_dbg,
                                NULL,   // dw_die
                                isInfo, // dw_is_info
                                &die,   // dw_return_sibling,
                                &error  // dw_error
        );
        if (IsError("dwarf_siblingof_b", res, error, dbg)) return FALSE;
        if (res == DW_DLV_NO_ENTRY) // this is not expected, it's an error
        {
            std::cerr << "Error: no die for compilation unit" << std::endl;
            return FALSE;
        }
        ASSERTX(res == DW_DLV_OK);

        int recursionLevel = 0;
        if (!TraverseDwarfTree(die, dbg, recursionLevel))
        {
            return FALSE;
        }
    }
    return TRUE;
}

/*
 * Print the subporograms in 'binary'
 */
static BOOL PrintDwarfSubprograms(const char* binary)
{
    Dwarf_Debug dbg = NULL;
    Dwarf_Error error;
    // Init
    int res = dwarf_init_path(binary,             // dw_path
                              NULL,               // dw_true_path_out_buffer
                              0,                  // dw_true_path_bufferlen,
                              DW_GROUPNUMBER_ANY, // dw_groupnumber
                              NULL,               // dw_errhand,
                              NULL,               // dw_errarg,
                              &dbg,               // dw_dbg
                              &error              // dw_error
    );
    if (IsError("dwarf_init_path", res, error, dbg)) return FALSE;

    BOOL succeeded = IterateOnCompilationUnits(dbg);

    res = dwarf_finish(dbg);
    if (IsError("dwarf_finish", res, error, dbg)) return FALSE;
    return succeeded;
}

/*
 * main
 */
int main(int argc, char* argv[])
{
    if (PIN_Init(argc, argv))
    {
        return Usage();
    }

    outfile.open(KnobOutputFile.Value().c_str());
    if (!outfile.is_open())
    {
        std::cout << "Could not open " << KnobOutputFile.Value() << std::endl;
        exit(1);
    }

    BOOL succeeded = PrintDwarfSubprograms(KnobBinary.Value().c_str());

    outfile.close();

    if (!succeeded)
    {
        PIN_ExitProcess(1);
    }
    return 0;
}
