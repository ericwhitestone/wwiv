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
#include "bbs/uedit.h"

#include <memory>
#include <string>

#include "bbs/bbs.h"
#include "bbs/bbsutl2.h"
#include "bbs/com.h"
#include "bbs/connect1.h"
#include "bbs/datetime.h"
#include "bbs/email.h"
#include "bbs/dupphone.h"
#include "bbs/finduser.h"
#include "bbs/inetmsg.h"
#include "bbs/input.h"
#include "bbs/keycodes.h"
#include "bbs/misccmd.h"
#include "bbs/newuser.h"
#include "bbs/smallrecord.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/shortmsg.h"
#include "bbs/vars.h"
#include "bbs/wconstants.h"
#include "bbs/wqscn.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "sdk/datetime.h"
#include "sdk/filenames.h"

using std::string;
using std::unique_ptr;
using namespace wwiv::bbs;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static uint32_t *u_qsc = nullptr;
static char *sp = nullptr;
static char search_pattern[81];

static void changeopt() {
  bout.cls();
  bout << "Current search string:\r\n";
  bout << ((search_pattern[0]) ? search_pattern : "-NONE-");
  bout.nl(3);
  bout << "|#9Change it? ";
  if (yesno()) {
    bout << "Enter new search pattern:\r\n|#7:";
    input(search_pattern, 75);
  }
}

static int matchuser(User *pUser) {
  int ok = 1;
  bool _not = 0, less = 0;
  int cpf = 0, cpp = 0;
  bool  _and = true, gotfcn = false;
  unsigned evalit = 0;
  unsigned int tmp2;
  char fcn[20], parm[80], ts[40];
  time_t l;

  bool done = false;
  bool tmp = false;
  unsigned tmp1 = 0;
  do {
    if (*sp == 0) {
      done = true;
    } else {
      if (strchr("()|&!<>", *sp)) {
        switch (*sp++) {
        case '(':
          evalit = 2;
          break;
        case ')':
          done = true;
          break;
        case '|':
          _and = false;
          break;
        case '&':
          _and = true;
          break;
        case '!':
          _not = true;
          break;
        case '<':
          less = true;
          break;
        case '>':
          less = false;
          break;
        }
      } else if (*sp == '[') {
        gotfcn = true;
        sp++;
      } else if (*sp == ']') {
        evalit = 1;
        ++sp;
      } else if (*sp != ' ' || gotfcn) {
        if (gotfcn) {
          if (cpp < 22) {
            parm[cpp++] = *sp++;
          } else {
            sp++;
          }
        } else {
          if (cpf < static_cast<int>(sizeof(fcn)) - 1) {
            fcn[cpf++] = *sp++;
          } else {
            sp++;
          }
        }
      } else {
        ++sp;
      }
      if (evalit) {
        if (evalit == 1) {
          fcn[cpf] = 0;
          parm[cpp] = 0;
          tmp = true;
          tmp1 = atoi(parm);

          if (IsEquals(fcn, "SL")) {
            if (less) {
              tmp = (tmp1 > pUser->GetSl());
            } else {
              tmp = (tmp1 < pUser->GetSl());
            }
          } else if (IsEquals(fcn, "DSL")) {
            if (less) {
              tmp = (tmp1 > pUser->GetDsl());
            } else {
              tmp = (tmp1 < pUser->GetDsl());
            }
          } else if (IsEquals(fcn, "AR")) {
            if (parm[0] >= 'A' && parm[0] <= 'P') {
              tmp1 = 1 << (parm[0] - 'A');
              tmp = pUser->HasArFlag(tmp1);
            } else {
              tmp = false;
            }
          } else if (IsEquals(fcn, "DAR")) {
            if ((parm[0] >= 'A') && (parm[0] <= 'P')) {
              tmp1 = 1 << (parm[0] - 'A');
              tmp = pUser->HasDarFlag(tmp1);
            } else {
              tmp = false;
            }
          } else if (IsEquals(fcn, "SEX")) {
            tmp = parm[0] == pUser->GetGender();
          } else if (IsEquals(fcn, "AGE")) {
            if (less) {
              tmp = (tmp1 > pUser->GetAge());
            } else {
              tmp = (tmp1 < pUser->GetAge());
            }
          } else if (IsEquals(fcn, "LASTON")) {
            time(&l);
            tmp2 = static_cast<unsigned int>((l - pUser->GetLastOnDateNumber()) / SECONDS_PER_DAY);
            if (less) {
              tmp = tmp2 < tmp1;
            } else {
              tmp = tmp2 > tmp1;
            }
          } else if (IsEquals(fcn, "AREACODE")) {
            tmp = !strncmp(parm, pUser->GetVoicePhoneNumber(), 3);
          } else if (IsEquals(fcn, "RESTRICT")) {
            ;
          } else if (IsEquals(fcn, "LOGONS")) {
            if (less) {
              tmp = pUser->GetNumLogons() < tmp1;
            } else {
              tmp = pUser->GetNumLogons() > tmp1;
            }
          } else if (IsEquals(fcn, "REALNAME")) {
            strcpy(ts, pUser->GetRealName());
            strupr(ts);
            tmp = (strstr(ts, parm) != nullptr);
          } else if (IsEquals(fcn, "BAUD")) {
            if (less) {
              tmp = pUser->GetLastBaudRate() < tmp1;
            } else {
              tmp = pUser->GetLastBaudRate() > tmp1;
            }

          } else if (IsEquals(fcn, "COMP_TYPE")) {
            tmp = pUser->GetComputerType() == static_cast<int>(tmp1);
          }
        } else {
          tmp = matchuser(pUser) > 0;
        }

        if (_not) {
          tmp = !tmp;
        }
        if (_and) {
          ok = ok && tmp;
        } else {
          ok = ok || tmp;
        }

        _not = less = gotfcn = false;
        cpf = cpp = evalit = 0;
        _and = true;
      }
    }
  } while (!done);
  return ok;
}

static int matchuser(int user_number) {
  User user;
  a()->users()->readuser(&user, user_number);
  sp = search_pattern;
  return matchuser(&user);
}

void deluser(int user_number) {
  User user;
  a()->users()->readuser(&user, user_number);

  if (!user.IsUserDeleted()) {
    rsm(user_number, &user, false);
    DeleteSmallRecord(user.GetName());
    user.SetInactFlag(User::userDeleted);
    user.SetNumMailWaiting(0);
    a()->users()->writeuser(&user, user_number);
    unique_ptr<File> pFileEmail(OpenEmailFile(true));
    if (pFileEmail->IsOpen()) {
      long lEmailFileLen = pFileEmail->length() / sizeof(mailrec);
      for (int nMailRecord = 0; nMailRecord < lEmailFileLen; nMailRecord++) {
        mailrec m;

        pFileEmail->Seek(nMailRecord * sizeof(mailrec), File::Whence::begin);
        pFileEmail->Read(&m, sizeof(mailrec));
        if ((m.tosys == 0 && m.touser == user_number) ||
            (m.fromsys == 0 && m.fromuser == user_number)) {
          delmail(*pFileEmail.get(), nMailRecord);
        }
      }
      pFileEmail->Close();
    }
    File voteFile(a()->config()->datadir(), VOTING_DAT);
    voteFile.Open(File::modeReadWrite | File::modeBinary);
    long nNumVoteRecords = static_cast<int>(voteFile.length() / sizeof(votingrec)) - 1;
    for (long lCurVoteRecord = 0; lCurVoteRecord < 20; lCurVoteRecord++) {
      if (user.GetVote(lCurVoteRecord)) {
        if (lCurVoteRecord <= nNumVoteRecords) {
          votingrec v;
          voting_response vr;

          voteFile.Seek(static_cast<long>(lCurVoteRecord * sizeof(votingrec)), File::Whence::begin);
          voteFile.Read(&v, sizeof(votingrec));
          vr = v.responses[ user.GetVote(lCurVoteRecord) - 1 ];
          vr.numresponses--;
          v.responses[ user.GetVote(lCurVoteRecord) - 1 ] = vr;
          voteFile.Seek(static_cast<long>(lCurVoteRecord * sizeof(votingrec)), File::Whence::begin);
          voteFile.Write(&v, sizeof(votingrec));
        }
        user.SetVote(lCurVoteRecord, 0);
      }
    }
    voteFile.Close();
    a()->users()->writeuser(&user, user_number);
    delete_phone_number(user_number, user.GetVoicePhoneNumber());   // dupphone addition
    delete_phone_number(user_number, user.GetDataPhoneNumber());    // dupphone addition
  }
}

void print_data(int user_number, User *pUser, bool bLongFormat, bool bClearScreen) {
  char s[81], s1[81], s2[81], s3[81];

  if (bClearScreen) {
    bout.cls();
  }
  if (pUser->IsUserDeleted()) {
    bout << "|#6>>> This User Is DELETED - Use 'R' to Restore <<<\r\n\n";
  }
  bout << "|#2N|#9) Name/Alias   : |#1" << pUser->GetName() << " #" << user_number << wwiv::endl;
  bout << "|#2L|#9) Real Name    : |#1" << pUser->GetRealName() << wwiv::endl;
  if (bLongFormat) {
    if (pUser->GetStreet()[0]) {
      bout << "|#2%|#9) Address      : |#1" << pUser->GetStreet() << wwiv::endl;
    }
    if (pUser->GetCity()[0] || pUser->GetState()[0] ||
        pUser->GetCountry()[0] || pUser->GetZipcode()[0]) {
      bout << "|#9   City         : |#1" << pUser->GetCity() << ", " <<
                         pUser->GetState() << "  " << pUser->GetZipcode() << " |#9(|#1" <<
                         pUser->GetCountry() << "|#9)\r\n";
    }
  }

  if (pUser->GetRegisteredDateNum() != 0) {
    bout << "|#2X|#9) Registration : |#1" << daten_to_mmddyy(pUser->GetRegisteredDateNum()) <<
                       "       Expires on   : " << daten_to_mmddyy(pUser->GetExpiresDateNum()) << wwiv::endl;
  }

  if (pUser->GetCallsign()[0] != '\0') {
    bout << "|#2C|#9) Call         : |#1" << pUser->GetCallsign() << wwiv::endl;
  }

  bout << "|#2P|#9) Voice Phone #: |#1" << pUser->GetVoicePhoneNumber() << wwiv::endl;
  if (pUser->GetDataPhoneNumber()[0] != '\0') {
    bout << "|#9   Data Phone # : |#1" << pUser->GetDataPhoneNumber() << wwiv::endl;
  }

  bout.bprintf("|#2G|#9) Birthday/Age : |#1(%02d/%02d/%02d) (Age: %d) (Gender: %c) \r\n",
                                    pUser->GetBirthdayMonth(), pUser->GetBirthdayDay(),
                                    pUser->GetBirthdayYear(), pUser->GetAge(), pUser->GetGender());

  bout << "|#2M|#9) Comp         : |#1"
       << (pUser->GetComputerType() == -1 ? "Waiting for answer" : ctypes(pUser->GetComputerType()))
       << wwiv::endl;
  if (pUser->GetForwardUserNumber() != 0) {
    bout << "|#9   Forwarded To : |#1";
    if (pUser->GetForwardSystemNumber() != 0) {
      if (a()->max_net_num() > 1) {
        bout << a()->net_networks[ pUser->GetForwardNetNumber() ].name 
             << " #" << pUser->GetForwardUserNumber()
             << " @" << pUser->GetForwardSystemNumber() << wwiv::endl;
      } else {
        bout << "#" << pUser->GetForwardUserNumber() << " @" << pUser->GetForwardSystemNumber() << wwiv::endl;
      }
    } else {
      bout << "#" << pUser->GetForwardUserNumber() << wwiv::endl;
    }
  }
  if (bLongFormat) {
    // TODO(rushfan): Should we always mask this?
    bout << "|#2H|#9) Password     : |#1";
    a()->localIO()->Puts(pUser->GetPassword());

    if (incom && a()->user()->GetSl() == 255) {
      bout.rputs(pUser->GetPassword());
    } else {
      bout.rputs("(not shown remotely)");
    }
    bout.nl();

    bout << "|#9   First/Last On: |#9(Last: |#1" << pUser->GetLastOn() << "|#9)   (First: |#1" <<
      pUser->GetFirstOn() << "|#9)\r\n";

    bout.bprintf("|#9   Message Stats: |#9(Post:|#1%u|#9) (Email:|#1%u|#9) (Fd:|#1%u|#9) (Wt:|#1%u|#9) (Net:|#1%u|#9) (Del:|#1%u|#9)\r\n",
      pUser->GetNumMessagesPosted(), pUser->GetNumEmailSent(),
      pUser->GetNumFeedbackSent(), pUser->GetNumMailWaiting(), pUser->GetNumNetEmailSent(), pUser->GetNumDeletedPosts());

    const string d = date();
    bout.bprintf("|#9   Call Stats   : |#9(Total: |#1%u|#9) (Today: |#1%d|#9) (Illegal: |#6%d|#9)\r\n",
      pUser->GetNumLogons(), (!IsEquals(pUser->GetLastOn(), d.c_str())) ? 0 : pUser->GetTimesOnToday(),
      pUser->GetNumIllegalLogons());

    bout.bprintf("|#9   Up/Dnld Stats: |#9(Up: |#1%u |#9files in |#1%lu|#9k)  (Dn: |#1%u |#9files in |#1%lu|#9k)\r\n",
      pUser->GetFilesUploaded(), pUser->GetUploadK(), pUser->GetFilesDownloaded(), pUser->GetDownloadK());

    bout << "|#9   Last Baud    : |#1" << pUser->GetLastBaudRate() << wwiv::endl;
  }
  if (pUser->GetNote()[0] != '\0') {
    bout << "|#2O|#9) Sysop Note   : |#1" << pUser->GetNote() << wwiv::endl;
  }
  if (pUser->GetAssPoints()) {
    bout << "|#9   Ass Points   : |#1" << pUser->GetAssPoints() << wwiv::endl;
  }
  bout << "|#2S|#9) SL           : |#1" << pUser->GetSl() << wwiv::endl;
  bout << "|#2T|#9) DSL          : |#1" << pUser->GetDsl() << wwiv::endl;
  if (u_qsc) {
    if ((*u_qsc) != 999) {
      bout << "|#9    Sysop Sub #: |#1" << *u_qsc << wwiv::endl;
    }
  }
  if (pUser->GetExempt() != 0) {
    bout.bprintf("|#9   Exemptions   : |#1%s %s %s %s %s (%d)\r\n",
                                      pUser->IsExemptRatio() ? "XFER" : "    ",
                                      pUser->IsExemptTime() ? "TIME" : "    ",
                                      pUser->IsExemptPost() ? "POST" : "    ",
                                      pUser->IsExemptAll() ? "ALL " : "    ",
                                      pUser->IsExemptAutoDelete() ? "ADEL" : "    ",
                                      pUser->GetExempt());
  }
  strcpy(s3, restrict_string);
  for (int i = 0; i <= 15; i++) {
    if (pUser->HasArFlag(1 << i)) {
      s[i] = static_cast<char>('A' + i);
    } else {
      s[i] = SPACE;
    }
    if (pUser->HasDarFlag(1 << i)) {
      s1[i] = static_cast<char>('A' + i);
    } else {
      s1[i] = SPACE;
    }
    if (pUser->GetRestriction() & (1 << i)) {
      s2[i] = s3[i];
    } else {
      s2[i] = SPACE;
    }
  }
  s[16]   = '\0';
  s1[16]  = '\0';
  s2[16]  = '\0';
  if (pUser->GetAr() != 0) {
    bout << "|#2A|#9) AR Flags     : |#1" << s << wwiv::endl;
  }
  if (pUser->GetDar() != 0) {
    bout << "|#2I|#9) DAR Flags    : |#1" << s1 << wwiv::endl;
  }
  if (pUser->GetRestriction() != 0) {
    bout << "|#2Z|#9) Restrictions : |#1" << s2 << wwiv::endl;
  }
  if (pUser->GetWWIVRegNumber()) {
    bout << "|#9   WWIV Reg Num : |#1" << pUser->GetWWIVRegNumber() << wwiv::endl;
  }
}

void auto_val(int n, User *pUser) {
  if (pUser->GetSl() == 255) {
    return;
  }
  pUser->SetSl(a()->config()->config()->autoval[n].sl);
  pUser->SetDsl(a()->config()->config()->autoval[n].dsl);
  pUser->SetAr(a()->config()->config()->autoval[n].ar);
  pUser->SetDar(a()->config()->config()->autoval[n].dar);
  pUser->SetRestriction(a()->config()->config()->autoval[n].restrict);
}
