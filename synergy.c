#include "synergy.h"




static void reply_add_string(struct synContext *ctx, const char *string)
{
	size_t len = strlen(string);
	memcpy(
