/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services            */
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

#ifndef __INCLUDED_USER_MANAGER_H__
#define __INCLUDED_USER_MANAGER_H__

#include <sstream>
#include <cstring>
#include <string>
#include "sdk/user.h"
#include "sdk/vardec.h"

namespace wwiv {
namespace sdk {

/**
 * WWIV User Manager.
 * 
 * Responsible for loading and saving users.
 */
class UserManager {
 private:
  const std::string data_directory_;
  int userrec_length_;
  int max_number_users_;
  bool allow_writes_ = false;
 public:
   UserManager() = delete;
   UserManager(std::string data_directory, int userrec_length, int max_number_users);
   virtual ~UserManager();
   int num_user_records() const;
   bool readuser_nocache(User *pUser, int user_number);
   bool readuser(User *pUser, int user_number);
   bool writeuser_nocache(User *pUser, int user_number);
   bool writeuser(User *pUser, int user_number);

  /**
   * Setting this to false will disable writing the userrecord to disk.  This should ONLY be false when the
   * Global guest_user variable is true.
   */
  void set_user_writes_allowed(bool bUserWritesAllowed) {
    allow_writes_ = bUserWritesAllowed;
  }
  bool user_writes_allowed() {
    return allow_writes_;
  }
};

}  // namespace sdk
}  // namespace wwiv

#endif // __INCLUDED_PLATFORM_WUSER_H__
