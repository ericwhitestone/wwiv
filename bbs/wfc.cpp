/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#include "bbs/wfc.h"

#include <cctype>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

// If this is gone, we get errors on CY and other things in wtypes.h
#include "core/stl.h"

#include "bbs/asv.h"
#include "bbs/bbsovl1.h"
#include "bbs/bbsovl2.h"
#include "bbs/bbsutl2.h"
#include "bbs/chnedit.h"
#include "bbs/bgetch.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/com.h"
#include "bbs/confutil.h"
#include "bbs/connect1.h"
#include "bbs/datetime.h"
#include "bbs/diredit.h"
#include "bbs/events.h"
#include "bbs/exceptions.h"
#include "bbs/external_edit.h"
#include "bbs/gfileedit.h"
#include "bbs/gfiles.h"
#include "bbs/input.h"
#include "bbs/inetmsg.h"
#include "bbs/instmsg.h"
#include "bbs/lilo.h"
#include "bbs/local_io.h"
#include "bbs/local_io_curses.h"
#include "bbs/multinst.h"
#include "bbs/null_local_io.h"
#include "bbs/null_remote_io.h"
#include "bbs/netsup.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/readmail.h"
#include "bbs/remote_io.h"
#include "bbs/subedit.h"
#include "bbs/ssh.h"
#include "bbs/sysopf.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/vars.h"
#include "bbs/voteedit.h"
#include "bbs/wconstants.h"
#include "bbs/wfc.h"
#include "bbs/wqscn.h"
#include "bbs/application.h"
#include "bbs/workspace.h"
#include "bbs/platform/platformfcns.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "core/wwivassert.h"
#include "core/wwivport.h"
#include "sdk/filenames.h"
#include "sdk/status.h"

using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::core::IniFile;
using wwiv::core::FilePath;
using wwiv::os::random_number;
using namespace std::chrono;
using namespace wwiv::sdk;
using namespace wwiv::strings;

// Local Functions
static char* pszScreenBuffer = nullptr;
static int inst_num;
static constexpr int sysop_usernum = 1;


void wfc_cls(Application* a) {
  if (a->HasConfigFlag(OP_FLAGS_WFC_SCREEN)) {
    bout.ResetColors();
    a->localIO()->Cls();
    a->localIO()->SetCursor(LocalIO::cursorNormal);
  }
  // Every time we clear the WFC, reset the lines listed.
  bout.clear_lines_listed();
}

namespace wwiv {
namespace bbs {

static void wfc_update() {
  // Every time we update the WFC, reset the lines listed.
  bout.clear_lines_listed();

  if (!a()->HasConfigFlag(OP_FLAGS_WFC_SCREEN)) {
    return;
  }

  instancerec ir = {};
  User u = {};

  get_inst_info(inst_num, &ir);
  a()->users()->readuser_nocache(&u, ir.user);
  a()->localIO()->PrintfXYA(57, 18, 15, "%-3d", inst_num);
  if (ir.flags & INST_FLAGS_ONLINE) {
    const string unn = a()->names()->UserName(ir.user);
    a()->localIO()->PrintfXYA(42, 19, 14, "%-25.25s", unn.c_str());
  } else {
    a()->localIO()->PrintfXYA(42, 19, 14, "%-25.25s", "Nobody");
  }

  string activity_string = make_inst_str(inst_num, INST_FORMAT_WFC);
  a()->localIO()->PrintfXYA(42, 20, 14, "%-25.25s", activity_string.c_str());
  if (num_instances() > 1) {
    do {
      ++inst_num;
      if (inst_num > num_instances()) {
        inst_num = 1;
      }
    } while (inst_num == a()->instance_number());
  }
}

void WFC::Clear() {
  wfc_cls(a_);
  status_ = 0;
}

void WFC::DrawScreen() {
  instancerec ir;
  User u;
  static steady_clock::time_point wfc_time;
  static steady_clock::time_point poll_time;

  if (!a()->HasConfigFlag(OP_FLAGS_WFC_SCREEN)) {
    return;
  }

  int nNumNewMessages = check_new_mail(sysop_usernum);
  std::unique_ptr<WStatus> pStatus(a()->status_manager()->GetStatus());
  if (status_ == 0) {
    a()->localIO()->SetCursor(LocalIO::cursorNone);
    a()->localIO()->Cls();
    if (pszScreenBuffer == nullptr) {
      pszScreenBuffer = new char[4000];
      File wfcFile(a()->config()->datadir(), WFC_DAT);
      if (!wfcFile.Open(File::modeBinary | File::modeReadOnly)) {
        Clear();
        LOG(FATAL) << wfcFile.full_pathname() << " NOT FOUND.";
        a()->AbortBBS();
      }
      wfcFile.Read(pszScreenBuffer, 4000);
    }
    a()->localIO()->WriteScreenBuffer(pszScreenBuffer);
    const string title = StringPrintf("Activity and Statistics of %s Node %d", 
      a()->config()->config()->systemname, a()->instance_number());
    a()->localIO()->PrintfXYA(1 + ((76 - title.size()) / 2), 4, 15, title.c_str());
    const string f = fulldate();
    a()->localIO()->PrintfXYA(8, 1, 14, f.c_str());
    std::string osVersion = wwiv::os::os_version_string();
    a()->localIO()->PrintfXYA(40, 1, 3, "OS: ");
    a()->localIO()->PrintfXYA(44, 1, 14, osVersion.c_str());
    a()->localIO()->PrintfXYA(21, 6, 14, "%d", pStatus->GetNumCallsToday());
    User sysop{};
    int feedback_waiting = 0;
    if (a()->users()->readuser_nocache(&sysop, sysop_usernum)) {
      feedback_waiting = sysop.GetNumMailWaiting();
    }
    a()->localIO()->PrintfXYA(21, 7, 14, "%d", feedback_waiting);
    if (nNumNewMessages) {
      a()->localIO()->PrintfXYA(29, 7 , 3, "New:");
      a()->localIO()->PrintfXYA(34, 7 , 12, "%d", nNumNewMessages);
    }
    a()->localIO()->PrintfXYA(21, 8, 14, "%d", pStatus->GetNumUploadsToday());
    a()->localIO()->PrintfXYA(21, 9, 14, "%d", pStatus->GetNumMessagesPostedToday());
    a()->localIO()->PrintfXYA(21, 10, 14, "%d", pStatus->GetNumLocalPosts());
    a()->localIO()->PrintfXYA(21, 11, 14, "%d", pStatus->GetNumEmailSentToday());
    a()->localIO()->PrintfXYA(21, 12, 14, "%d", pStatus->GetNumFeedbackSentToday());
    a()->localIO()->PrintfXYA(21, 13, 14, "%d Mins (%.1f%%)", pStatus->GetMinutesActiveToday(),
                                            100.0 * static_cast<float>(pStatus->GetMinutesActiveToday()) / 1440.0);
    a()->localIO()->PrintfXYA(58, 6, 14, "%s%s", wwiv_version, beta_version);

    a()->localIO()->PrintfXYA(58, 7, 14, "%d", pStatus->GetNetworkVersion());
    a()->localIO()->PrintfXYA(58, 8, 14, "%d", pStatus->GetNumUsers());
    a()->localIO()->PrintfXYA(58, 9, 14, "%ld", pStatus->GetCallerNumber());
    if (pStatus->GetDays()) {
      a()->localIO()->PrintfXYA(58, 10, 14, "%.2f", static_cast<float>(pStatus->GetCallerNumber()) /
                                              static_cast<float>(pStatus->GetDays()));
    } else {
      a()->localIO()->PrintfXYA(58, 10, 14, "N/A");
    }
    a()->localIO()->PrintfXYA(58, 11, 14, sysop2() ? "Available    " : "Not Available");

    get_inst_info(a()->instance_number(), &ir);
    if (ir.user < a()->config()->config()->maxusers && ir.user > 0) {
      const string unn = a()->names()->UserName(ir.user);
      a()->localIO()->PrintfXYA(33, 16, 14, "%-20.20s", unn.c_str());
    } else {
      a()->localIO()->PrintfXYA(33, 16, 14, "%-20.20s", "Nobody");
    }

    status_ = 1;
    wfc_update();
    poll_time = wfc_time = steady_clock::now();
  } else {
    auto screen_saver_time = seconds(a()->screen_saver_time);
    if ((a()->screen_saver_time == 0) 
        || (steady_clock::now() - wfc_time < screen_saver_time)) {
      string t = times();
      a()->localIO()->PrintfXYA(28, 1, 14, t.c_str());
      a()->localIO()->PrintfXYA(58, 11, 14, sysop2() ? "Available    " : "Not Available");
      if (steady_clock::now() - poll_time > seconds(10)) {
        wfc_update();
        poll_time = steady_clock::now();
      }
    } else {
      if ((steady_clock::now() - poll_time > seconds(10)) || status_ == 1) {
        status_ = 2;
        a_->localIO()->Cls();
        a()->localIO()->PrintfXYA(
            random_number(38), random_number(24), random_number(14) + 1,
            "WWIV Screen Saver - Press Any Key For WWIV");
        wfc_time = steady_clock::now() - seconds(a()->screen_saver_time) - seconds(1);
        poll_time = steady_clock::now();
      }
    }
  }
}

WFC::WFC(Application* a) : a_(a) {
  a_->localIO()->SetCursor(LocalIO::cursorNormal);
  if (a_->HasConfigFlag(OP_FLAGS_WFC_SCREEN)) {
    status_ = 0;
    inst_num = 1;
  }
  Clear();
}

WFC::~WFC() {
}

int WFC::doWFCEvents() {
  unsigned char ch = 0;
  int lokb = 0;
  LocalIO* io = a_->localIO();

  unique_ptr<WStatus> last_date_status(a_->status_manager()->GetStatus());
  do {
    write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);
    a_->set_net_num(0);
    bool any = false;
    a_->set_at_wfc(true);

    // If the date has changed since we last checked, then then run the beginday event.
    if (date() != last_date_status->GetLastDate()) {
      if ((a_->GetBeginDayNodeNumber() == 0) || (a_->instance_number() == a_->GetBeginDayNodeNumber())) {
        cleanup_events();
        beginday(true);
        Clear();
      }
    }

    if (!do_event) {
      check_event();
    }

    while (do_event) {
      run_event(do_event - 1);
      // dunno if we really need this.
      Clear();
      check_event();
      any = true;
    }

    lokb = 0;
    a_->SetCurrentSpeed("KB");
    auto current_time = steady_clock::now();
    bool node_supports_callout = a_->HasConfigFlag(OP_FLAGS_NET_CALLOUT);
    // try to check for packets to send every minute.
    auto diff_time = current_time - last_network_attempt();
    bool time_to_call = diff_time > minutes(1);  // was 1200
    if (!any && time_to_call && a_->current_net().sysnum && node_supports_callout) {
      // also try this.
      Clear();
      attempt_callout();
      any = true;
    }
    DrawScreen();
    okskey = false;
    if (io->KeyPressed()) {
      a_->set_at_wfc(false);
      a_->ReadCurrentUser(sysop_usernum);
      read_qscn(1, qsc, false);
      a_->set_at_wfc(true);
      ch = to_upper_case<char>(io->GetChar());
      if (ch == 0) {
        ch = io->GetChar();
        a_->handle_sysop_key(ch);
        ch = 0;
      }
    }
    else {
      ch = 0;
      giveup_timeslice();
    }
    if (ch) {
      a_->set_at_wfc(true);
      any = true;
      okskey = true;
      resetnsp();
      io->SetCursor(LocalIO::cursorNormal);
      switch (ch) {
        // Local Logon
      case SPACE:
        lokb = LocalLogon();
        break;
        // Show WFC Menu
      case '?': {
        string helpFileName = SWFC_NOEXT;
        char chHelp = ESC;
        do {
          io->Cls();
          bout.nl();
          printfile(helpFileName);
          chHelp = bout.getkey();
          helpFileName = (helpFileName == SWFC_NOEXT) ? SONLINE_NOEXT : SWFC_NOEXT;
        } while (chHelp != SPACE && chHelp != ESC);
      }
      break;
      // Force Network Callout
      case '/':
        if (a_->current_net().sysnum) {
          force_callout();
        }
        break;
        // War Dial Connect
        // Fast Net Callout from WFC
      case '*': {
        io->Cls();
        do_callout(32767);
      } break;
      case ',':
        // Print NetLogs
        if (a_->current_net().sysnum > 0 || !a_->net_networks.empty()) {
          io->GotoXY(2, 23);
          bout << "|#7(|#2Q|#7=|#2Quit|#7) Display Which NETDAT Log File (|#10|#7-|#12|#7): ";
          ch = onek("Q012");
          switch (ch) {
          case '0':
          case '1':
          case '2': {
            print_local_file(StringPrintf("netdat%c.log", ch));
          } break;
          }
        }
        break;
        // Net List
      case '`':
        if (a_->current_net().sysnum) {
          print_net_listing(true);
        }
        break;
        // [ESC] Quit the BBS
      case ESC:
        io->GotoXY(2, 23);
        bout << "|#7Exit the BBS? ";
        if (yesno()) {
          a_->QuitBBS();
        }
        io->Cls();
        break;
        // BoardEdit
      case 'B':
        write_inst(INST_LOC_BOARDEDIT, 0, INST_FLAGS_NONE);
        boardedit();
        cleanup_net();
        break;
        // ChainEdit
      case 'C':
        write_inst(INST_LOC_CHAINEDIT, 0, INST_FLAGS_NONE);
        chainedit();
        break;
        // DirEdit
      case 'D':
        write_inst(INST_LOC_DIREDIT, 0, INST_FLAGS_NONE);
        dlboardedit();
        break;
        // Send Email
      case 'E':
        Clear();
        a_->usernum = 1;
        bout.bputs("|#1Send Email:");
        send_email();
        a_->WriteCurrentUser(sysop_usernum);
        cleanup_net();
        break;
        // GfileEdit
      case 'G':
        write_inst(INST_LOC_GFILEEDIT, 0, INST_FLAGS_NONE);
        gfileedit();
        break;
        // EventEdit
      case 'H':
        write_inst(INST_LOC_EVENTEDIT, 0, INST_FLAGS_NONE);
        eventedit();
        break;
        // Send Internet Mail
      case 'I': {
        Clear();
        a_->usernum = 1;
        a_->SetUserOnline(true);
        get_user_ppp_addr();
        send_inet_email();
        a_->SetUserOnline(false);
        a_->WriteCurrentUser(sysop_usernum);
        cleanup_net();
      }
      break;
      // ConfEdit
      case 'J':
        Clear();
        edit_confs();
        break;
        // SendMailFile
      case 'K': {
        Clear();
        a_->usernum = 1;
        bout << "|#1Send any Text File in Email:\r\n\n|#2Filename: ";
        string buffer = input(50);
        LoadFileIntoWorkspace(buffer, false);
        send_email();
        a_->WriteCurrentUser(sysop_usernum);
        cleanup_net();
      }
      break;
      // Print Log Daily logs
      case 'L': {
        Clear();
        unique_ptr<WStatus> pStatus(a_->status_manager()->GetStatus());
        print_local_file(pStatus->GetLogFileName(0));
      }
      break;
      // Read User Mail
      case 'M': {
        Clear();
        a_->usernum = sysop_usernum;
        readmail(0);
        a_->WriteCurrentUser(sysop_usernum);
        cleanup_net();
      }
      break;
      // Print Net Log
      case 'N': {
        Clear();
        print_local_file("net.log");
      }
      break;
      // EditTextFile
      case 'O': {
        Clear();
        write_inst(INST_LOC_TEDIT, 0, INST_FLAGS_NONE);
        bout << "\r\n|#1Edit any Text File: \r\n\n|#2Filename: ";
        const string current_dir_slash = File::current_directory() + File::pathSeparatorString;
        string newFileName = Input1(current_dir_slash, 50, true, InputMode::FULL_PATH_NAME);
        if (!newFileName.empty()) {
          external_text_edit(newFileName, "", 500, ".", MSGED_FLAG_NO_TAGLINE);
        }
      }
      break;
      // Print Network Pending list
      case 'P': {
        Clear();
        print_pending_list();
      }
      break;
      // Quit BBS
      case 'Q':
        io->GotoXY(2, 23);
        a_->QuitBBS();
        break;
        // Read All Mail
      case 'R':
        Clear();
        write_inst(INST_LOC_MAILR, 0, INST_FLAGS_NONE);
        mailr();
        break;
        // Print Current Status
      case 'S':
        prstatus();
        bout.getkey();
        break;
      case 'T':
        if (a()->terminal_command.empty()) {
          bout << "Terminal Command not specified. " << wwiv::endl << " Please set TERMINAL_CMD in WWIV.INI"
            << wwiv::endl;
          bout.getkey();
          break;
        }
        ExecExternalProgram(a()->terminal_command, INST_FLAGS_NONE);
        break;
      case 'U': {
        // User edit
        const auto exe = FilePath(a()->config()->root_directory(), "init");
        const auto cmd = StrCat(exe, " --user_editor");
        ExecExternalProgram(cmd, INST_FLAGS_NONE);
      } break;
      case 'V': {
        // InitVotes
        Clear();
        write_inst(INST_LOC_VOTEEDIT, 0, INST_FLAGS_NONE);
        ivotes();
      }
      break;
      // Edit Gfile
      case 'W': {
        Clear();
        write_inst(INST_LOC_TEDIT, 0, INST_FLAGS_NONE);
        bout << "|#1Edit " << a()->config()->gfilesdir() << "<filename>: \r\n";
        text_edit();
      }
      break;
      // Print Environment
      case 'X':
        break;
        // Print Yesterday's Log
      case 'Y': {
        Clear();
        unique_ptr<WStatus> pStatus(a_->status_manager()->GetStatus());
        print_local_file(pStatus->GetLogFileName(1));
      }
      break;
      // Print Activity (Z) Log
      case 'Z': {
        zlog();
        bout.nl();
        bout.getkey();
      } break;
      }
      Clear();  // moved from after getch
      if (!incom && !lokb) {
        frequent_init();
        a_->ReadCurrentUser(sysop_usernum);
        read_qscn(1, qsc, false);
        a_->ResetEffectiveSl();
        a_->usernum = sysop_usernum;
      }
      catsl();
      write_inst(INST_LOC_WFC, 0, INST_FLAGS_NONE);
    }

    if (!any) {
      static steady_clock::time_point mult_time;
      auto now = steady_clock::now();
      auto diff = now - mult_time;
      if (a_->IsCleanNetNeeded() || diff > seconds(54)) {
        // let's try this.
        Clear();
        cleanup_net();
        mult_time = steady_clock::now();
      }
      giveup_timeslice();
    }
  } while (!incom && !lokb);
  return lokb;
}

int WFC::LocalLogon() {
  a_->localIO()->GotoXY(2, 23);
  bout << "|#9Log on to the BBS?";
  auto d = steady_clock::now();
  int lokb = 0;
  // TODO(rushfan): use wwiv::os::wait_for
  while (!a_->localIO()->KeyPressed() && (steady_clock::now() - d < minutes(1)))
    ;

  if (a_->localIO()->KeyPressed()) {
    char ch = to_upper_case<char>(a_->localIO()->GetChar());
    if (ch == 'Y') {
      a_->localIO()->Puts(YesNoString(true));
      bout << wwiv::endl;
      lokb = 1;
    }
    else if (ch == 0 || static_cast<unsigned char>(ch) == 224) {
      // The ch == 224 is a Win32'ism
      a_->localIO()->GetChar();
    }
    else {
      bool fast = false;

      if (ch == 'F') {   // 'F' for Fast
        a_->unx_ = 1;
        fast = true;
      }
      else {
        switch (ch) {
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          fast = true;
          a_->unx_ = ch - '0';
          break;
        }
      }
      if (!fast || a_->unx_ > a_->status_manager()->GetUserCount()) {
        return lokb;
      }

      User tu;
      a_->users()->readuser_nocache(&tu, a_->unx_);
      if (tu.GetSl() != 255 || tu.IsUserDeleted()) {
        return lokb;
      }

      a_->usernum = a_->unx_;
      bool saved_at_wfc = a_->at_wfc();
      a_->set_at_wfc(false);
      a_->ReadCurrentUser();
      read_qscn(a_->usernum, qsc, false);
      a_->set_at_wfc(saved_at_wfc);
      bout.bputch(ch);
      a_->localIO()->Puts("\r\n\r\n\r\n\r\n\r\n\r\n");
      lokb = 2;
      a_->ResetEffectiveSl();
      changedsl();
      if (!set_language(a_->user()->GetLanguage())) {
        a_->user()->SetLanguage(0);
        set_language(0);
      }
      return lokb;
    }
    if (ch == 0 || static_cast<unsigned char>(ch) == 224) {
      // The 224 is a Win32'ism
      a_->localIO()->GetChar();
    }
  }
  if (lokb == 0) {
    a_->localIO()->Cls();
  }
  return lokb;
}

}
}