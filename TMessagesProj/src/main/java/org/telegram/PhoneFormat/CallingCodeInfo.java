// Copyright (c) 2012, Rick Maddy

// * Redistributions of source code must retain the above copyright notice, this

// * Redistributions in binary form must reproduce the above copyright notice,

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND

// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE

package org.telegram.PhoneFormat;

import java.util.ArrayList;

public class CallingCodeInfo {
    public ArrayList<String> countries = new ArrayList<>();
    public String callingCode = "";
    public ArrayList<String> trunkPrefixes = new ArrayList<>();
    public ArrayList<String> intlPrefixes = new ArrayList<>();
    public ArrayList<RuleSet> ruleSets = new ArrayList<>();

    String matchingAccessCode(String str) {
        for (String code : intlPrefixes) {
            if (str.startsWith(code)) {
                return code;
            }
        }
        return null;
    }

    String matchingTrunkCode(String str) {
        for (String code : trunkPrefixes) {
            if (str.startsWith(code)) {
                return code;
            }
        }

        return null;
    }

    String format(String orig) {
        String str = orig;
        String trunkPrefix = null;
        String intlPrefix = null;
        if (str.startsWith(callingCode)) {
            intlPrefix = callingCode;
            str = str.substring(intlPrefix.length());
        } else {
            String trunk = matchingTrunkCode(str);
            if (trunk != null) {
                trunkPrefix = trunk;
                str = str.substring(trunkPrefix.length());
            }
        }

        for (RuleSet set : ruleSets) {
            String phone = set.format(str, intlPrefix, trunkPrefix, true);
            if (phone != null) {
                return phone;
            }
        }

        for (RuleSet set : ruleSets) {
            String phone = set.format(str, intlPrefix, trunkPrefix, false);
            if (phone != null) {
                return phone;
            }
        }

        if (intlPrefix != null && str.length() != 0) {
            return String.format("%s %s", intlPrefix, str);
        }

        return orig;
    }

    boolean isValidPhoneNumber(String orig) {
        String str = orig;
        String trunkPrefix = null;
        String intlPrefix = null;
        if (str.startsWith(callingCode)) {
            intlPrefix = callingCode;
            str = str.substring(intlPrefix.length());
        } else {
            String trunk = matchingTrunkCode(str);
            if (trunk != null) {
                trunkPrefix = trunk;
                str = str.substring(trunkPrefix.length());
            }
        }

        for (RuleSet set : ruleSets) {
            boolean valid = set.isValid(str, intlPrefix, trunkPrefix, true);
            if (valid) {
                return true;
            }
        }

        for (RuleSet set : ruleSets) {
            boolean valid = set.isValid(str, intlPrefix, trunkPrefix, false);
            if (valid) {
                return true;
            }
        }

        return false;
    }
}
