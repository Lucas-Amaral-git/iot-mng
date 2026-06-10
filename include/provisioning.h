// provisioning.h
#pragma once

#include "storage.h"

void startProvisioningAP(unsigned long timeoutMs = 600000UL);
bool isProvisioningActive();
void stopProvisioning();
void provisioningLoop();