// provisioning.h
#pragma once

#include "storage.h"

void startProvisioningAP();
bool isProvisioningActive();
void stopProvisioning();
void provisioningLoop();