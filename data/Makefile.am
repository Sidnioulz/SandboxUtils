# Dbus service file
servicedir = $(DBUS_SERVICES_DIR)
systemd_servicedir = $(SYSTEMD_USER_DIR)
#systemd_confdir = $(SYSTEMD_SYSTEM_DIR)/user@.service.d/
service_in_files = org.mupuf.SecureFileAccess.service.in
service_DATA = $(service_in_files:.service.in=.service)
systemd_service_in_files = SecureFileAccess.service.in
systemd_service_DATA = $(systemd_service_in_files:.service.in=.service)
#systemd_conf_DATA = environment.conf
EXTRA_DIST = $(service_in_files) $(systemd_service_in_files)
#EXTRA_DIST = $(service_in_files) $(systemd_service_in_files) $(systemd_conf_DATA)

# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@bindir\@|$(bindir)|g" $<> $@

# Rule to make the systemd service file
$(systemd_service_DATA): $(systemd_service_in_files) Makefile
	@sed -e "s|\@bindir\@|$(bindir)|g" $<> $@

clean-local:
	rm -rf $(service_DATA)
