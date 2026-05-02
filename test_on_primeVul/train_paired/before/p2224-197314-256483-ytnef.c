void MAPIPrint(MAPIProps *p) {
  int j, i, index, h, x;
  DDWORD *ddword_ptr;
  DDWORD ddword_tmp;
  dtr thedate;
  MAPIProperty *mapi;
  variableLength *mapidata;
  variableLength vlTemp;
  int found;

  for (j = 0; j < p->count; j++) {
    mapi = &(p->properties[j]);
    printf("   #%i: Type: [", j);
    switch (PROP_TYPE(mapi->id)) {
      case PT_UNSPECIFIED:
        printf("  NONE   "); break;
      case PT_NULL:
        printf("  NULL   "); break;
      case PT_I2:
        printf("   I2    "); break;
      case PT_LONG:
        printf("  LONG   "); break;
      case PT_R4:
        printf("   R4    "); break;
      case PT_DOUBLE:
        printf(" DOUBLE  "); break;
      case PT_CURRENCY:
        printf("CURRENCY "); break;
      case PT_APPTIME:
        printf("APP TIME "); break;
      case PT_ERROR:
        printf("  ERROR  "); break;
      case PT_BOOLEAN:
        printf(" BOOLEAN "); break;
      case PT_OBJECT:
        printf(" OBJECT  "); break;
      case PT_I8:
        printf("   I8    "); break;
      case PT_STRING8:
        printf(" STRING8 "); break;
      case PT_UNICODE:
        printf(" UNICODE "); break;
      case PT_SYSTIME:
        printf("SYS TIME "); break;
      case PT_CLSID:
        printf("OLE GUID "); break;
      case PT_BINARY:
        printf(" BINARY  "); break;
      default:
        printf("<%x>", PROP_TYPE(mapi->id)); break;
    }

    printf("]  Code: [");
    if (mapi->custom == 1) {
      printf("UD:x%04x", PROP_ID(mapi->id));
    } else {
      found = 0;
      for (index = 0; index < sizeof(MPList) / sizeof(MAPIPropertyTagList); index++) {
        if ((MPList[index].id == PROP_ID(mapi->id)) && (found == 0)) {
          printf("%s", MPList[index].name);
          found = 1;
        }
      }
      if (found == 0) {
        printf("0x%04x", PROP_ID(mapi->id));
      }
    }
    printf("]\n");
    if (mapi->namedproperty > 0) {
      for (i = 0; i < mapi->namedproperty; i++) {
        printf("    Name: %s\n", mapi->propnames[i].data);
      }
    }
    for (i = 0; i < mapi->count; i++) {
      mapidata = &(mapi->data[i]);
      if (mapi->count > 1) {
        printf("    [%i/%u] ", i, mapi->count);
      } else {
        printf("    ");
      }
      printf("Size: %i", mapidata->size);
      switch (PROP_TYPE(mapi->id)) {
        case PT_SYSTIME:
          MAPISysTimetoDTR(mapidata->data, &thedate);
          printf("    Value: ");
          ddword_tmp = *((DDWORD *)mapidata->data);
          TNEFPrintDate(thedate);
          printf(" [HEX: ");
          for (x = 0; x < sizeof(ddword_tmp); x++) {
            printf(" %02x", (BYTE)mapidata->data[x]);
          }
          printf("] (%llu)\n", ddword_tmp);
          break;
        case PT_LONG:
          printf("    Value: %li\n", *((long*)mapidata->data));
          break;
        case PT_I2:
          printf("    Value: %hi\n", *((short int*)mapidata->data));
          break;
        case PT_BOOLEAN:
          if (mapi->data->data[0] != 0) {
            printf("    Value: True\n");
          } else {
            printf("    Value: False\n");
          }
          break;
        case PT_OBJECT:
          printf("\n");
          break;
        case PT_BINARY:
          if (IsCompressedRTF(mapidata) == 1) {
            printf("    Detected Compressed RTF. ");
            printf("Decompressed text follows\n");
            printf("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
            if ((vlTemp.data = (BYTE*)DecompressRTF(mapidata, &(vlTemp.size))) != NULL) {
              printf("%s\n", vlTemp.data);
              free(vlTemp.data);
            }
            printf("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
          } else {
            printf("    Value: [");
            for (h = 0; h < mapidata->size; h++) {
              if (isprint(mapidata->data[h])) {
                printf("%c", mapidata->data[h]);
              } else {
                printf(".");
              }

            }
            printf("]\n");
          }
          break;
        case PT_STRING8:
          printf("    Value: [%s]\n", mapidata->data);
          if (strlen((char*)mapidata->data) != mapidata->size - 1) {
            printf("Detected Hidden data: [");
            for (h = 0; h < mapidata->size; h++) {
              if (isprint(mapidata->data[h])) {
                printf("%c", mapidata->data[h]);
              } else {
                printf(".");
              }

            }
            printf("]\n");
          }
          break;
        case PT_CLSID:
          printf("    Value: ");
          printf("[HEX: ");
          for(x=0; x< 16; x++) {
            printf(" %02x", (BYTE)mapidata->data[x]);
          }
          printf("]\n");
          break;
        default:
          printf("    Value: [%s]\n", mapidata->data);
      }
    }
  }
}