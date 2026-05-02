    void publish(Topic *iterator, size_t start, size_t stop, std::string_view topic, std::pair<std::string_view, std::string_view> message) {

        /* Iterate over all segments in given topic */
        for (; stop != std::string::npos; start = stop + 1) {
            stop = topic.find('/', start);
            std::string_view segment = topic.substr(start, stop - start);

            /* It is very important to disallow wildcards when publishing.
             * We will not catch EVERY misuse this lazy way, but enough to hinder
             * explosive recursion.
             * Terminating wildcards MAY still get triggered along the way, if for
             * instace the error is found late while iterating the topic segments. */
            if (segment.length() == 1) {
                if (segment[0] == '+' || segment[0] == '#') {
                    return;
                }
            }

            /* Do we have a terminating wildcard child? */
            if (iterator->terminatingWildcardChild) {
                iterator->terminatingWildcardChild->messages[messageId] = message;

                /* Add this topic to triggered */
                if (!iterator->terminatingWildcardChild->triggered) {
                    /* If we already have 64 triggered topics make sure to drain it here */
                    if (numTriggeredTopics == 64) {
                        drain();
                    }

                    triggeredTopics[numTriggeredTopics++] = iterator->terminatingWildcardChild;
                    iterator->terminatingWildcardChild->triggered = true;
                }
            }

            /* Do we have a wildcard child? */
            if (iterator->wildcardChild) {
                publish(iterator->wildcardChild, stop + 1, stop, topic, message);
            }

            std::map<std::string_view, Topic *>::iterator it = iterator->children.find(segment);
            if (it == iterator->children.end()) {
                /* Stop trying to match by exact string */
                return;
            }

            iterator = it->second;
        }

        /* If we went all the way we matched exactly */
        iterator->messages[messageId] = message;

        /* Add this topic to triggered */
        if (!iterator->triggered) {
            /* If we already have 64 triggered topics make sure to drain it here */
            if (numTriggeredTopics == 64) {
                drain();
            }

            triggeredTopics[numTriggeredTopics++] = iterator;
            iterator->triggered = true;
        }
    }