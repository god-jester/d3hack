.PHONY: deploy-sd deploy-ftp deploy-ryu deploy-yuzu ryu-launch-log ryu-tail ryu-screenshot

deploy-sd:
	@$(SHELL) $(SCRIPTS_PATH)/deploy-sd.sh

deploy-ftp:
	@$(PYTHON) $(SCRIPTS_PATH)/deploy-ftp.py

deploy-ryu:
	@$(SHELL) $(SCRIPTS_PATH)/deploy-ryu.sh

deploy-yuzu:
	@$(SHELL) $(SCRIPTS_PATH)/deploy-yuzu.sh

ryu-launch-log:
	@$(SHELL) $(SCRIPTS_PATH)/ryu-launch-log.sh

ryu-tail:
	@$(MAKE) -j all >/dev/null && \
		$(MAKE) deploy-ryu && \
		$(MAKE) ryu-launch-log

ryu-screenshot:
	@/bin/bash $(SCRIPTS_PATH)/ryu-screenshot.sh
