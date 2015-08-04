FENCE_OBJS := $(SCFENCE_DIR)/scgen.o \
	$(SCFENCE_DIR)/inference.o \
	$(SCFENCE_DIR)/inferset.o \
	$(SCFENCE_DIR)/inferlist.o \
	$(SCFENCE_DIR)/patch.o \
	$(SCFENCE_DIR)/scfence.o

$(FENCE_OBJS): $(SCFENCE_DIR)/%.o : $(SCFENCE_DIR)/%.cc $(SCFENCE_DIR)/%.h
	$(CXX) -MMD -MF $(SCFENCE_DIR)/.$(notdir $@).d -fPIC -c $< $(CPPFLAGS) -o $@

OBJECTS += $(FENCE_OBJS)
