# Tests hôte du firmware.
#
#     make            compile et exécute
#     make clean
#
# Ces tests ne demandent ni ESP-IDF, ni matériel, ni ROS : un compilateur C
# suffit. C'est délibéré — un test qu'on ne peut lancer qu'avec le robot sur
# l'établi n'est pas lancé.

CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -Werror -O2
INCLUDES = -I../protocol/generated -I../components/retriever_link/portable
PORTABLE = ../components/retriever_link/portable
BUILD    = build

SRCS_FRAMING = $(PORTABLE)/rt_cobs.c $(PORTABLE)/rt_crc16.c $(PORTABLE)/rt_framing.c

.PHONY: all run clean vectors

all: run

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/test_protocol: test_protocol.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) $< -lm -o $@

$(BUILD)/test_framing: test_framing.c $(SRCS_FRAMING) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@

$(BUILD)/emit_vectors: emit_vectors.c $(SRCS_FRAMING) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@

vectors: $(BUILD)/emit_vectors
	@$(BUILD)/emit_vectors

run: $(BUILD)/test_protocol $(BUILD)/test_framing
	@echo "=== protocole ==="   && $(BUILD)/test_protocol
	@echo "=== cadrage ==="     && $(BUILD)/test_framing

clean:
	rm -rf $(BUILD)
