static int bus_socket_make_message(sd_bus *bus, size_t size) {
        sd_bus_message *t;
        void *b;
        int r;

        assert(bus);
        assert(bus->rbuffer_size >= size);
        assert(IN_SET(bus->state, BUS_RUNNING, BUS_HELLO));

        r = bus_rqueue_make_room(bus);
        if (r < 0)
                return r;

        if (bus->rbuffer_size > size) {
                b = memdup((const uint8_t*) bus->rbuffer + size,
                           bus->rbuffer_size - size);
                if (!b)
                        return -ENOMEM;
        } else
                b = NULL;

        r = bus_message_from_malloc(bus,
                                    bus->rbuffer, size,
                                    bus->fds, bus->n_fds,
                                    NULL,
                                    &t);
        if (r < 0) {
                free(b);
                return r;
        }

        bus->rbuffer = b;
        bus->rbuffer_size -= size;

        bus->fds = NULL;
        bus->n_fds = 0;

        bus->rqueue[bus->rqueue_size++] = t;

        return 1;
}