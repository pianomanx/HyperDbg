/**
 * @file ScriptEngineHeader.h
 * @author Sina Karvandi (sina@hyperdbg.org)
 * @brief Headers for exported script engine headers
 * @details
 * @version 0.2
 * @date 2022-06-28
 *
 * @copyright This project is released under the GNU Public License v3.
 *
 */
#pragma once

//////////////////////////////////////////////////
//			        Registers                   //
//////////////////////////////////////////////////

BOOLEAN
SetRegValue(PGUEST_REGS GuestRegs, UINT32 RegisterId, UINT64 Value);

UINT64
GetRegValue(PGUEST_REGS GuestRegs, REGS_ENUM RegId);

//////////////////////////////////////////////////
//			        Functions                   //
//////////////////////////////////////////////////

BOOL
ScriptEngineExecute(PGUEST_REGS                    GuestRegs,
                    ACTION_BUFFER *                ActionDetail,
                    SCRIPT_ENGINE_VARIABLES_LIST * VariablesList,
                    SYMBOL_BUFFER *                CodeBuffer,
                    UINT64 *                       Indx,
                    SYMBOL_BUFFER *                StackBuffer,
                    UINT64 *                       StackIndx,
                    UINT64 *                       StackBaseIndx,
                    SYMBOL *                       ErrorOperator,
                    UINT64 *                       ReturnValue);

UINT64
GetRegValue(PGUEST_REGS GuestRegs, REGS_ENUM RegId);

VOID
ScriptEngineGetOperatorName(PSYMBOL OperatorSymbol, CHAR * BufferForName);

VOID
ScriptEngineGetOperatorName(PSYMBOL OperatorSymbol, CHAR * BufferForName);
