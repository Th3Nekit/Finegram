// Copyright 2015 Google Inc. All Rights Reserved.

// Licensed under the Apache License, Version 2.0 (the "License");

package org.telegram.messenger.support.customtabsclient.shared;

import org.telegram.messenger.support.customtabs.CustomTabsClient;

public interface ServiceConnectionCallback {

    void onServiceConnected(CustomTabsClient client);

    void onServiceDisconnected();
}
