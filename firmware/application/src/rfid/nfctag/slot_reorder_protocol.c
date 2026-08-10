#include "slot_reorder_protocol.h"

#include <stddef.h>

#include "app_status.h"
#include "data_cmd.h"
#include "tag_emulation.h"

tag_slot_swap_protocol_response_t tag_slot_swap_protocol_process(
    uint16_t command,
    uint16_t length,
    const uint8_t *data,
    tag_slot_swap_callback_t callback,
    void *context
) {
    tag_slot_swap_protocol_response_t response = {
        .command = command,
        .status = STATUS_PAR_ERR,
    };

    if (command != DATA_CMD_SWAP_SLOTS) {
        response.status = STATUS_INVALID_CMD;
        return response;
    }
    if (length != 2 || data == NULL || callback == NULL ||
            data[0] >= TAG_MAX_SLOT_NUM || data[1] >= TAG_MAX_SLOT_NUM) {
        return response;
    }

    response.status = callback(data[0], data[1], context)
        ? STATUS_SUCCESS
        : STATUS_FLASH_WRITE_FAIL;
    return response;
}
