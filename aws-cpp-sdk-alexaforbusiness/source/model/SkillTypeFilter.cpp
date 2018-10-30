﻿/*
* Copyright 2010-2017 Amazon.com, Inc. or its affiliates. All Rights Reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License").
* You may not use this file except in compliance with the License.
* A copy of the License is located at
*
*  http://aws.amazon.com/apache2.0
*
* or in the "license" file accompanying this file. This file is distributed
* on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
* express or implied. See the License for the specific language governing
* permissions and limitations under the License.
*/

#include <aws/alexaforbusiness/model/SkillTypeFilter.h>
#include <aws/core/utils/HashingUtils.h>
#include <aws/core/Globals.h>
#include <aws/core/utils/EnumParseOverflowContainer.h>

using namespace Aws::Utils;


namespace Aws
{
  namespace AlexaForBusiness
  {
    namespace Model
    {
      namespace SkillTypeFilterMapper
      {

        static const int PUBLIC__HASH = HashingUtils::HashString("PUBLIC");
        static const int PRIVATE__HASH = HashingUtils::HashString("PRIVATE");
        static const int ALL_HASH = HashingUtils::HashString("ALL");


        SkillTypeFilter GetSkillTypeFilterForName(const Aws::String& name)
        {
          int hashCode = HashingUtils::HashString(name.c_str());
          if (hashCode == PUBLIC__HASH)
          {
            return SkillTypeFilter::PUBLIC_;
          }
          else if (hashCode == PRIVATE__HASH)
          {
            return SkillTypeFilter::PRIVATE_;
          }
          else if (hashCode == ALL_HASH)
          {
            return SkillTypeFilter::ALL;
          }
          EnumParseOverflowContainer* overflowContainer = Aws::GetEnumOverflowContainer();
          if(overflowContainer)
          {
            overflowContainer->StoreOverflow(hashCode, name);
            return static_cast<SkillTypeFilter>(hashCode);
          }

          return SkillTypeFilter::NOT_SET;
        }

        Aws::String GetNameForSkillTypeFilter(SkillTypeFilter enumValue)
        {
          switch(enumValue)
          {
          case SkillTypeFilter::PUBLIC_:
            return "PUBLIC";
          case SkillTypeFilter::PRIVATE_:
            return "PRIVATE";
          case SkillTypeFilter::ALL:
            return "ALL";
          default:
            EnumParseOverflowContainer* overflowContainer = Aws::GetEnumOverflowContainer();
            if(overflowContainer)
            {
              return overflowContainer->RetrieveOverflow(static_cast<int>(enumValue));
            }

            return "";
          }
        }

      } // namespace SkillTypeFilterMapper
    } // namespace Model
  } // namespace AlexaForBusiness
} // namespace Aws
