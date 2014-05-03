systemd_servicedir = $(SYSTEMD_USER_DIR)
#systemd_confdir = $(SYSTEMD_SYSTEM_DIR)/user@.service.d/
systemd_service_in_files = SandboxUtils.service.in
systemd_service_DATA = $(systemd_service_in_files:.service.in=.service)
#systemd_conf_DATA = environment.conf
EXTRA_DIST = $(systemd_service_in_files)
#EXTRA_DIST = $(service_in_files) $(systemd_service_in_files) $(systemd_conf_DATA)

# Rule to make the systemd service file
$(systemd_service_DATA): $(systemd_service_in_files) Makefile
	@sed -e "s|\@bindir\@|$(bindir)|g" $<> $@

clean-local:
	rm -rf $(systemd_service_DATA)
