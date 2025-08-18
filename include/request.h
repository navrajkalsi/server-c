#pragma once

#include "client.h"

int handle_request(Client *client);

int verify_method(Str *request);

int verify_path(Str *request);
