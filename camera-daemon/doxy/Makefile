all: docs

docs: html

html: logs
	# run the html output
	ALIAS_INCLUDE_FILE=template/other_html_aliases.txt \
   ENV_WARN_LOG=logs/html_doxy_warn.log \
   ENV_GEN_HTML=YES \
   ENV_GEN_LATEX=NO \
   ENV_LAYOUT_FILE=template/Doxygen_Layout_Standard_html.xml \
   doxygen Doxy_Config.ldd > logs/html_doxy.log

logs:
	mkdir -p logs

clean:
	rm -rf logs
	rm -rf html
	rm -rf latex
