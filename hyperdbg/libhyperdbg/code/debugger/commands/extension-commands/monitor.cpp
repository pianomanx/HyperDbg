/**
 * @file monitor.cpp
 * @author Sina Karvandi (sina@hyperdbg.org)
 * @brief !monitor command
 * @details
 * @version 0.1
 * @date 2020-05-27
 *
 * @copyright This project is released under the GNU Public License v3.
 *
 */
#include "pch.h"

/**
 * @brief help of the !monitor command
 *
 * @return VOID
 */
VOID
CommandMonitorHelp()
{
    ShowMessages("!monitor : monitors address range for read and writes.\n\n");

    ShowMessages("syntax : \t!monitor [MemoryType (vapa)] [Attribute (string)] [FromAddress (hex)] "
                 "[ToAddress (hex)] [pid ProcessId (hex)] [core CoreId (hex)] "
                 "[imm IsImmediate (yesno)] [sc EnableShortCircuiting (onoff)] [stage CallingStage (prepostall)] "
                 "[buffer PreAllocatedBuffer (hex)] [script { Script (string) }] [asm condition { Condition (assembly/hex) }] "
                 "[asm code { Code (assembly/hex) }] [output {OutputName (string)}]\n");

    ShowMessages("syntax : \t!monitor [MemoryType (vapa)] [Attribute (string)] [FromAddress (hex)] "
                 "[l Length (hex)] [pid ProcessId (hex)] [core CoreId (hex)] "
                 "[imm IsImmediate (yesno)] [sc EnableShortCircuiting (onoff)] [stage CallingStage (prepostall)] "
                 "[buffer PreAllocatedBuffer (hex)] [script { Script (string) }] [asm condition { Condition (assembly/hex) }] "
                 "[asm code { Code (assembly/hex) }] [output {OutputName (string)}]\n");

    ShowMessages("\n");
    ShowMessages("\t\te.g : !monitor rw fffff801deadb000 fffff801deadbfff\n");
    ShowMessages("\t\te.g : !monitor rw fffff801deadb000 l 1000\n");
    ShowMessages("\t\te.g : !monitor pa rw c01000 l 1000\n");
    ShowMessages("\t\te.g : !monitor rwx fffff801deadb000 fffff801deadbfff\n");
    ShowMessages("\t\te.g : !monitor rwx fffff801deadb000 l 230d0\n");
    ShowMessages("\t\te.g : !monitor rw nt!Kd_DEFAULT_Mask Kd_DEFAULT_Mask+5\n");
    ShowMessages("\t\te.g : !monitor r fffff801deadb000 fffff801deadbfff pid 400\n");
    ShowMessages("\t\te.g : !monitor w fffff801deadb000 fffff801deadbfff core 2 pid 400\n");
    ShowMessages("\t\te.g : !monitor w c01000 c01000+2500 core 2 pid 400\n");
    ShowMessages("\t\te.g : !monitor x fffff801deadb000 fffff801deadbfff core 2 pid 400\n");
    ShowMessages("\t\te.g : !monitor x fffff801deadb000 l 500 core 2 pid 400\n");
    ShowMessages("\t\te.g : !monitor wx fffff801deadb000 fffff801deadbfff core 2 pid 400\n");
    ShowMessages("\t\te.g : !monitor rw fffff801deadb000 l 1000 script { printf(\"read/write occurred at the virtual address: %%llx\\n\", $context); }\n");
    ShowMessages("\t\te.g : !monitor rw fffff801deadb000 l 1000 asm code { nop; nop; nop }\n");
}

/**
 * @brief !monitor command handler
 *
 * @param SplitCommand
 * @param Command
 * @return VOID
 */
VOID
CommandMonitor(vector<string> SplitCommand, string Command)
{
    PDEBUGGER_GENERAL_EVENT_DETAIL     Event                 = NULL;
    PDEBUGGER_GENERAL_ACTION           ActionBreakToDebugger = NULL;
    PDEBUGGER_GENERAL_ACTION           ActionCustomCode      = NULL;
    PDEBUGGER_GENERAL_ACTION           ActionScript          = NULL;
    UINT32                             EventLength;
    UINT32                             ActionBreakToDebuggerLength = 0;
    UINT32                             ActionCustomCodeLength      = 0;
    UINT32                             ActionScriptLength          = 0;
    UINT32                             HookLength                  = 0;
    UINT64                             OptionalParam1              = 0; // Set the 'from' target address
    UINT64                             OptionalParam2              = 0; // Set the 'to' target address
    BOOLEAN                            SetFrom                     = FALSE;
    BOOLEAN                            SetTo                       = FALSE;
    BOOLEAN                            IsNextLength                = FALSE;
    BOOLEAN                            LengthAlreadySet            = FALSE;
    BOOLEAN                            SetAttributes               = FALSE;
    BOOLEAN                            HookMemoryTypeSet           = FALSE;
    vector<string>                     SplitCommandCaseSensitive {Split(Command, ' ')};
    DEBUGGER_HOOK_MEMORY_TYPE          HookMemoryType              = DEBUGGER_MEMORY_HOOK_VIRTUAL_ADDRESS; // by default virtual address
    UINT32                             IndexInCommandCaseSensitive = 0;
    DEBUGGER_EVENT_PARSING_ERROR_CAUSE EventParsingErrorCause;

    if (SplitCommand.size() < 4)
    {
        ShowMessages("incorrect use of the '!monitor'\n");
        CommandMonitorHelp();
        return;
    }

    //
    // Interpret and fill the general event and action fields
    //
    // We use HIDDEN_HOOK_READ_AND_WRITE here but it might be changed to
    // HIDDEN_HOOK_READ or HIDDEN_HOOK_WRITE or other events it is because
    // we are not sure what kind event the user need
    //
    if (!InterpretGeneralEventAndActionsFields(
            &SplitCommand,
            &SplitCommandCaseSensitive,
            HIDDEN_HOOK_READ_AND_WRITE_AND_EXECUTE,
            &Event,
            &EventLength,
            &ActionBreakToDebugger,
            &ActionBreakToDebuggerLength,
            &ActionCustomCode,
            &ActionCustomCodeLength,
            &ActionScript,
            &ActionScriptLength,
            &EventParsingErrorCause))
    {
        return;
    }

    //
    // Interpret command specific details (if any)
    //
    for (auto Section : SplitCommand)
    {
        IndexInCommandCaseSensitive++;

        if (!Section.compare("!monitor"))
        {
            continue;
        }
        else if (IsNextLength)
        {
            if (!ConvertStringToUInt32(Section, &HookLength))
            {
                ShowMessages("err, you should enter a valid length\n\n");
                return;
            }

            IsNextLength     = FALSE;
            LengthAlreadySet = TRUE;
            SetTo            = TRUE; // No longer need a second address
        }
        else if (!Section.compare("r") && !SetAttributes)
        {
            Event->EventType = HIDDEN_HOOK_READ;
            SetAttributes    = TRUE;
        }
        else if (!Section.compare("w") && !SetAttributes)
        {
            Event->EventType = HIDDEN_HOOK_WRITE;
            SetAttributes    = TRUE;
        }
        else if (!Section.compare("x") && !SetAttributes)
        {
            Event->EventType = HIDDEN_HOOK_EXECUTE;
            SetAttributes    = TRUE;
        }
        else if ((!Section.compare("rw") || !Section.compare("wr")) && !SetAttributes)
        {
            Event->EventType = HIDDEN_HOOK_READ_AND_WRITE;
            SetAttributes    = TRUE;
        }
        else if ((!Section.compare("rx") || !Section.compare("xr")) &&
                 !SetAttributes)
        {
            Event->EventType = HIDDEN_HOOK_READ_AND_EXECUTE;
            SetAttributes    = TRUE;
        }
        else if ((!Section.compare("wx") || !Section.compare("xw")) &&
                 !SetAttributes)
        {
            Event->EventType = HIDDEN_HOOK_WRITE_AND_EXECUTE;
            SetAttributes    = TRUE;
        }
        else if ((!Section.compare("rwx") ||
                  !Section.compare("rxw") ||
                  !Section.compare("wrx") ||
                  !Section.compare("wxr") ||
                  !Section.compare("xrw") ||
                  !Section.compare("xwr")) &&
                 !SetAttributes)
        {
            Event->EventType = HIDDEN_HOOK_READ_AND_WRITE_AND_EXECUTE;
            SetAttributes    = TRUE;
        }
        else if (!Section.compare("l") && !SetTo && !LengthAlreadySet)
        {
            IsNextLength = TRUE;
            continue;
        }
        else if (!Section.compare("va") && !HookMemoryTypeSet)
        {
            HookMemoryType    = DEBUGGER_MEMORY_HOOK_VIRTUAL_ADDRESS;
            HookMemoryTypeSet = TRUE;
            continue;
        }
        else if (!Section.compare("pa") && !HookMemoryTypeSet)
        {
            HookMemoryType    = DEBUGGER_MEMORY_HOOK_PHYSICAL_ADDRESS;
            HookMemoryTypeSet = TRUE;
            continue;
        }
        else
        {
            //
            // It's probably address
            //
            if (!SetFrom)
            {
                if (!SymbolConvertNameOrExprToAddress(
                        SplitCommandCaseSensitive.at(IndexInCommandCaseSensitive - 1),
                        &OptionalParam1))
                {
                    //
                    // couldn't resolve or unknown parameter
                    //
                    ShowMessages("err, couldn't resolve error at '%s'\n\n",
                                 SplitCommandCaseSensitive.at(IndexInCommandCaseSensitive - 1).c_str());
                    CommandMonitorHelp();

                    FreeEventsAndActionsMemory(Event, ActionBreakToDebugger, ActionCustomCode, ActionScript);
                    return;
                }
                SetFrom = TRUE;
            }
            else if (!SetTo && !LengthAlreadySet)
            {
                if (!SymbolConvertNameOrExprToAddress(
                        SplitCommandCaseSensitive.at(IndexInCommandCaseSensitive - 1),
                        &OptionalParam2))
                {
                    //
                    // Couldn't resolve or unknown parameter
                    //
                    ShowMessages("err, couldn't resolve error at '%s'\n\n",
                                 SplitCommandCaseSensitive.at(IndexInCommandCaseSensitive - 1).c_str());

                    CommandMonitorHelp();

                    FreeEventsAndActionsMemory(Event, ActionBreakToDebugger, ActionCustomCode, ActionScript);
                    return;
                }
                SetTo = TRUE;
            }
            else
            {
                //
                // Unknown parameter
                //
                ShowMessages("unknown parameter '%s'\n\n", Section.c_str());
                CommandMonitorHelp();

                FreeEventsAndActionsMemory(Event, ActionBreakToDebugger, ActionCustomCode, ActionScript);
                return;
            }
        }
    }

    //
    // Check  if all parameters are received
    //
    if (!SetFrom || !SetTo)
    {
        ShowMessages("please choose the 'from' or 'to' values or specify the length\n");
        FreeEventsAndActionsMemory(Event, ActionBreakToDebugger, ActionCustomCode, ActionScript);
        return;
    }

    //
    // Check if user specified the 'l' rather than providing two addresses
    //
    if (LengthAlreadySet)
    {
        //
        // Because when the user specifies length, the last byte should be ignored
        //
        OptionalParam2 = OptionalParam1 + HookLength - 1;
    }

    //
    // Check for invalid order of address
    //
    if (OptionalParam1 > OptionalParam2)
    {
        //
        // 'from' is greater than 'to'
        //
        ShowMessages("please choose the 'from' value first, then choose the 'to' "
                     "value\n");

        FreeEventsAndActionsMemory(Event, ActionBreakToDebugger, ActionCustomCode, ActionScript);
        return;
    }

    //
    // Check if user set the attributes of !monitor or not
    //
    if (!SetAttributes)
    {
        ShowMessages("please specify the attribute(s) that you want to monitor (r, w, x, rw, rx, wx, rwx)\n");

        FreeEventsAndActionsMemory(Event, ActionBreakToDebugger, ActionCustomCode, ActionScript);
        return;
    }

    //
    // Set the optional parameters
    //
    Event->Options.OptionalParam1 = OptionalParam1;
    Event->Options.OptionalParam2 = OptionalParam2;
    Event->Options.OptionalParam3 = HookMemoryType;

    //
    // Send the ioctl to the kernel for event registration
    //
    if (!SendEventToKernel(Event, EventLength))
    {
        //
        // There was an error, probably the handle was not initialized
        // we have to free the Action before exit, it is because, we
        // already freed the Event and string buffers
        //

        FreeEventsAndActionsMemory(Event, ActionBreakToDebugger, ActionCustomCode, ActionScript);
        return;
    }

    //
    // Add the event to the kernel
    //
    if (!RegisterActionToEvent(Event,
                               ActionBreakToDebugger,
                               ActionBreakToDebuggerLength,
                               ActionCustomCode,
                               ActionCustomCodeLength,
                               ActionScript,
                               ActionScriptLength))
    {
        //
        // There was an error
        //

        FreeEventsAndActionsMemory(Event, ActionBreakToDebugger, ActionCustomCode, ActionScript);
        return;
    }
}
