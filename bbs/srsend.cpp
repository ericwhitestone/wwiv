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
#include <chrono>
#include <cmath>
#include <string>

#include "bbs/crc.h"
#include "bbs/datetime.h"
#include "bbs/keycodes.h"
#include "bbs/remote_io.h"
#include "bbs/bbs.h"
#include "bbs/com.h"
#include "bbs/sr.h"
#include "bbs/utility.h"
#include "bbs/vars.h"
#include "bbs/xfer.h"
#include "core/os.h"
#include "core/strings.h"

using std::string;
using namespace std::chrono;
using namespace wwiv::os;
using namespace wwiv::strings;

bool NewZModemSendFile(const char *file_name);

#if (_MSC_VER >= 1900)
#define timezone _timezone
#endif  // MSV_VER && !timezone

void send_block(char *b, int block_type, bool use_crc, char byBlockNumber) {
  int nBlockSize = 0;

  CheckForHangup();
  switch (block_type) {
  case 5:
    nBlockSize = 128;
    bout.rputch(1);
    break;
  case 4:
    bout.rputch('\x81');
    bout.rputch(byBlockNumber);
    bout.rputch(byBlockNumber ^ 0xff);
    break;
  case 3:
    bout.rputch(CX);
    break;
  case 2:
    bout.rputch(4);
    break;
  case 1:
    nBlockSize = 1024;
    bout.rputch(2);
    break;
  case 0:
    nBlockSize = 128;
    bout.rputch(1);
  }
  if (block_type > 1 && block_type < 5) {
    return;
  }

  bout.rputch(byBlockNumber);
  bout.rputch(byBlockNumber ^ 0xff);
  crc = 0;
  checksum = 0;
  for (int i = 0; i < nBlockSize; i++) {
    char ch = b[i];
    bout.rputch(ch);
    calc_CRC(ch);
  }

  if (use_crc) {
    bout.rputch(static_cast<char>(crc >> 8));
    bout.rputch(static_cast<char>(crc & 0x00ff));
  } else {
    bout.rputch(checksum);
  }
  bout.dump();
}

char send_b(File &file, long pos, int block_type, char byBlockNumber, bool *use_crc, const char *file_name,
            int *terr, bool *abort) {
  char b[1025], szTempBuffer[20];

  int nb = 0;
  if (block_type == 0) {
    nb = 128;
  }
  if (block_type == 1) {
    nb = 1024;
  }
  if (nb) {
    file.Seek(pos, File::Whence::begin);
    auto nNumRead = file.Read(b, nb);
    for (int i = nNumRead; i < nb; i++) {
      b[i] = '\0';
    }
  } else if (block_type == 5) {
    char szFileDate[20];
    memset(b, 0, 128);
    nb = 128;
    strcpy(b, stripfn(file_name));
    sprintf(szTempBuffer, "%ld ", pos);
    // We needed this cast to (long) to compile with XCode 1.5 on OS X
    sprintf(szFileDate, "%ld", static_cast<long>(file.last_write_time() - timezone));

    strcat(szTempBuffer, szFileDate);
    strcpy(&(b[strlen(b) + 1]), szTempBuffer);
    b[127] = static_cast<unsigned char>((static_cast<int>(pos + 127) / 128) >> 8);
    b[126] = static_cast<unsigned char>((static_cast<int>(pos + 127) / 128) & 0x00ff);
  }
  bool done = false;
  int nNumErrors = 0;
  char ch = 0;
  do {
    send_block(b, block_type, *use_crc, byBlockNumber);
    ch = gettimeout(5, abort);
    if (ch == 'C' && pos == 0) {
      *use_crc = true;
    }
    if (ch == 6 || ch == CX) {
      done = true;
    } else {
      ++nNumErrors;
      ++(*terr);
      if (nNumErrors >= 9) {
        done = true;
      }
      a()->localIO()->PrintfXY(69, 4, "%d", nNumErrors);
      a()->localIO()->PrintfXY(69, 5, "%d", *terr);
    }
  } while (!done && !hangup && !*abort);

  if (ch == 6) {
    return 6;
  }
  if (ch == CX) {
    return CX;
  }
  return CU;
}

bool okstart(bool *use_crc, bool *abort) {
  auto d = steady_clock::now();
  bool ok = false;
  bool done = false;

  seconds s90(90);
  while (steady_clock::now() - d < s90 && !done && !hangup && !*abort) {
    char ch = gettimeout(91, abort);
    if (ch == 'C') {
      *use_crc = true;
      ok = true;
      done = true;
    }
    if (ch == CU) {
      *use_crc = false;
      ok = true;
      done = true;
    }
    if (ch == CX) {
      ok = false;
      done = true;
    }
  }
  return ok;
}

static int GetXYModemBlockSize(bool bBlockSize1K) {
  return (bBlockSize1K) ? 1024 : 128;
}

void xymodem_send(const char *file_name, bool *sent, double *percent, bool use_crc, bool use_ymodem,
                  bool use_ymodemBatch) {
  char ch;

  long cp = 0L;
  char byBlockNumber = 1;
  bool abort = false;
  int terr = 0;
  char *pszWorkingFileName = strdup(file_name);
  File file(pszWorkingFileName);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    if (!use_ymodemBatch) {
      bout << "\r\nFile not found.\r\n\n";
    }
    *sent = false;
    *percent = 0.0;
    return;
  }
  auto lFileSize = file.length();
  if (!lFileSize) {
    lFileSize = 1;
  }

  double tpb = (12.656f / static_cast<double>(modem_speed));

  if (!use_ymodemBatch) {
    bout << "\r\n-=> Beginning file transmission, Ctrl+X to abort.\r\n";
  }
  int xx1 = a()->localIO()->WhereX();
  int yy1 = a()->localIO()->WhereY();
  a()->localIO()->PutsXY(52, 0, "\xB3 Filename :               ");
  a()->localIO()->PutsXY(52, 1, "\xB3 Xfer Time:               ");
  a()->localIO()->PutsXY(52, 2, "\xB3 File Size:               ");
  a()->localIO()->PutsXY(52, 3, "\xB3 Cur Block: 1 - 1k        ");
  a()->localIO()->PutsXY(52, 4, "\xB3 Consec Errors: 0         ");
  a()->localIO()->PutsXY(52, 5, "\xB3 Total Errors : 0         ");
  a()->localIO()->PutsXY(52, 6,
                                       "\xC0\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4");
  a()->localIO()->PutsXY(65, 0, stripfn(pszWorkingFileName));
  a()->localIO()->PrintfXY(65, 2, "%ld - %ldk", (lFileSize + 127) / 128, bytes_to_k(lFileSize));

  if (!okstart(&use_crc, &abort)) {
    abort = true;
  }
  if (use_ymodem && !abort && !hangup) {
    ch = send_b(file, lFileSize, 5, 0, &use_crc, pszWorkingFileName, &terr, &abort);
    if (ch == CX) {
      abort = true;
    }
    if (ch == CU) {
      send_b(file, 0L, 3, 0, &use_crc, pszWorkingFileName, &terr, &abort);
      abort = true;
    }
  }
  bool bUse1kBlocks = false;
  while (!hangup && !abort && cp < lFileSize) {
    bUse1kBlocks = (use_ymodem) ? true : false;
    if ((lFileSize - cp) < 128L) {
      bUse1kBlocks = false;
    }
    a()->localIO()->PrintfXY(65, 3, "%ld - %ldk", cp / 128 + 1, cp / 1024 + 1);
    const string t = ctim(std::lround((lFileSize - cp) * tpb));
    a()->localIO()->PutsXY(65, 1, t.c_str());
    a()->localIO()->PutsXY(69, 4, "0");

    ch = send_b(file, cp, (bUse1kBlocks) ? 1 : 0, byBlockNumber, &use_crc, pszWorkingFileName, &terr, &abort);
    if (ch == CX) {
      abort = true;
    } else if (ch == CU) {
      sleep_for(seconds(1));
      bout.dump();
      send_b(file, 0L, 3, 0, &use_crc, pszWorkingFileName, &terr, &abort);
      abort = true;
    } else {
      ++byBlockNumber;
      cp += GetXYModemBlockSize(bUse1kBlocks);
    }
  }
  if (!hangup && !abort) {
    send_b(file, 0L, 2, 0, &use_crc, pszWorkingFileName, &terr, &abort);
  }
  if (!abort) {
    *sent = true;
    *percent = 1.0;
  } else {
    *sent = false;
    cp += GetXYModemBlockSize(bUse1kBlocks);
    if (cp >= lFileSize) {
      *percent = 1.0;
    } else {
      cp -= GetXYModemBlockSize(bUse1kBlocks);
      *percent = ((double)(cp)) / ((double) lFileSize);
    }
  }
  file.Close();
  a()->localIO()->GotoXY(xx1, yy1);
  if (*sent && !use_ymodemBatch) {
    bout << "-=> File transmission complete.\r\n\n";
  }
  free(pszWorkingFileName);
}

void zmodem_send(const string& file_name, bool *sent, double *percent) {
  *sent = false;
  *percent = 0.0;

  string s = file_name;
  StringRemoveWhitespace(&s);

  bool bOldBinaryMode = a()->remoteIO()->binary_mode();
  a()->remoteIO()->set_binary_mode(true);
  bool bResult = NewZModemSendFile(s.c_str());
  a()->remoteIO()->set_binary_mode(bOldBinaryMode);

  if (bResult) {
    *sent = true;
    *percent = 100.0;
  }
}

