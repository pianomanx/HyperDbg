/**
 * @file breakcontrol.cpp
 * @author Sina Karvandi (sina@rayanfam.com)
 * @brief break control is the handler for CTRL+C and CTRL+BREAK Signals
 * @details
 * @version 0.1
 * @date 2020-07-24
 *
 * @copyright This project is released under the GNU Public License v3.
 *
 */
#include "pch.h"

//
// Global Variables
//
extern BOOLEAN g_BreakPrintingOutput;
extern BOOLEAN g_AutoUnpause;

/**
 * @brief handle CTRL+C and CTRL+Break events
 * 
 * @param CtrlType 
 * @return BOOL 
 */
BOOL BreakController(DWORD CtrlType) {
  switch (CtrlType) {

    //
    // Handle the CTRL-C signal.
    //
  case CTRL_C_EVENT:

    //
    // Sleep because the other thread that shows must be stopped
    //
    g_BreakPrintingOutput = TRUE;

    Sleep(500);
    if (g_AutoUnpause) {
      ShowMessages("pause\npausing debugger...\nauto-unpause mode is enabled, "
                   "debugger will automatically continue when you run a new "
                   "event command, if you want to change this behaviour then "
                   "run run 'settings autounpause off'\n\nHyperDbg >");
    } else {
      ShowMessages(
          "pause\npausing debugger...\nauto-unpause mode is disabled, you "
          "should run 'g' when you want to continue, otherwise run 'settings "
          "autounpause on'\n\nHyperDbg >");
    }
    return TRUE;

    //
    // CTRL-CLOSE: confirm that the user wants to exit.
    //
  case CTRL_CLOSE_EVENT:
    return TRUE;

    //
    // Pass other signals to the next handler.
    //
  case CTRL_BREAK_EVENT:

    //
    // Sleep because the other thread that shows must be stopped
    //
    g_BreakPrintingOutput = TRUE;

    Sleep(500);
    if (g_AutoUnpause) {
      ShowMessages("pause\npausing debugger...\nauto-unpause mode is enabled, "
                   "debugger will automatically continue when you run a new "
                   "event command, if you want to change this behaviour then "
                   "run run 'settings autounpause off'\n\nHyperDbg >");
    } else {
      ShowMessages(
          "pause\npausing debugger...\nauto-unpause mode is disabled, you "
          "should run 'g' when you want to continue, otherwise run 'settings "
          "autounpause on'\n\nHyperDbg >");
    }
    return TRUE;

  case CTRL_LOGOFF_EVENT:
    return FALSE;

  case CTRL_SHUTDOWN_EVENT:
    return FALSE;

  default:

    //
    // Return TRUE if handled this message, further handler functions won't be
    // called.
    // Return FALSE to pass this message to further handlers until default
    // handler calls ExitProcess().
    //
    return FALSE;
  }
}
