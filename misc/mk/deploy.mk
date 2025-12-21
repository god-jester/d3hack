.PHONY: deploy-sd deploy-ftp deploy-ryu deploy-yuzu

deploy-sd:
	@$(SHELL) $(SCRIPTS_PATH)/deploy-sd.sh

deploy-ftp:
	@$(PYTHON) $(SCRIPTS_PATH)/deploy-ftp.py

deploy-ryu:
	@$(SHELL) $(SCRIPTS_PATH)/deploy-ryu.sh

deploy-yuzu:
	@$(SHELL) $(SCRIPTS_PATH)/deploy-yuzu.sh
