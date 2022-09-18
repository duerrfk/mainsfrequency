#include "tlv.h"

int read_tlv(tlv_t *tlv, FILE *f)
{
     size_t nread;

     nread = fread(&tlv->type, sizeof(tlv->type), 1, f);
     if (nread != 1)
	  return -1;

     nread = fread(&tlv->length, sizeof(tlv->length), 1, f);
     if (nread != 1)
	  return -1;

     nread = fread(&tlv->value, 1, tlv->length, f);
     if (nread != tlv->length)
	  return -1;
     
     return 0;
}

int write_tlv(const tlv_t *tlv, FILE *f)
{
     size_t nwritten;
     
     nwritten = fwrite(tlv, sizeof(tlv->type) + sizeof(tlv->length) + tlv->length, 1, f);
     if (nwritten != 1)
	  return -1;

     return 0;
}
