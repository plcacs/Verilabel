parseNormalModeParameters(IsoPresentation* self, uint8_t* buffer, int totalLength, int bufPos)
{
    int endPos = bufPos + totalLength;

    self->calledPresentationSelector.size = 0;
    self->callingPresentationSelector.size = 0;

    bool hasUserData = false;

    while (bufPos < endPos) {
        uint8_t tag = buffer[bufPos++];
        int len;

        if (bufPos == endPos) {
            if (DEBUG_PRES)
                printf("PRES: invalid message\n");
            return -1;
        }

        bufPos = BerDecoder_decodeLength(buffer, &len, bufPos, endPos);

        if (bufPos < 0) {
            if (DEBUG_PRES)
                printf("PRES: wrong parameter length\n");
            return -1;
        }

        switch (tag) {
        case 0x81: /* calling-presentation-selector */

            if (len > 16) {
                if (DEBUG_PRES)
                    printf("PRES: calling-presentation-sel too large\n");
            }
            else {
                self->callingPresentationSelector.size = len;
                int i;
                for (i = 0; i < len; i++)
                    self->callingPresentationSelector.value[i] = buffer[bufPos + i];
            }

            bufPos += len;
            break;

        case 0x82: /* called-presentation-selector */

            if (len > 16) {
                if (DEBUG_PRES)
                    printf("PRES: called-presentation-sel too large\n");
            }
            else {
                self->calledPresentationSelector.size = len;
                int i;
                for (i = 0; i < len; i++)
                    self->calledPresentationSelector.value[i] = buffer[bufPos + i];
            }

            bufPos += len;
            break;

        case 0x83: /* responding-presentation-selector */

            if (len > 16) {
                if (DEBUG_PRES)
                    printf("PRES: responding-presentation-sel too large\n");
            }

            bufPos += len;
            break;

        case 0xa4: /* presentation-context-definition list */
            if (DEBUG_PRES)
                printf("PRES: pcd list\n");
            bufPos = parsePresentationContextDefinitionList(self, buffer, len, bufPos);
            break;

        case 0xa5: /* context-definition-result-list */

            bufPos += len;
            break;

        case 0x61: /* user data */
            if (DEBUG_PRES)
                printf("PRES: user-data\n");

            bufPos = parseFullyEncodedData(self, buffer, len, bufPos);

            if (bufPos < 0)
                return -1;

            if (self->nextPayload.size > 0)
                hasUserData = true;

            break;

        case 0x00: /* indefinite length end tag -> ignore */
            break;

        default:
            if (DEBUG_PRES)
                printf("PRES: unknown tag in normal-mode\n");
            bufPos += len;
            break;
        }
    }

    if (hasUserData == false) {
        if (DEBUG_PRES)
            printf("PRES: user-data is missing\n");

        return -1;
    }

    return bufPos;
}