<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<title>Wireless Router <#Web_Title#> - <#menu5_4_4#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">

<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/wcdma_list.js"></script>
<script type="text/javascript" src="/cdma2000_list.js"></script>
<script type="text/javascript" src="/td-scdma_list.js"></script>
<script type="text/javascript" src="/wimax_list.js"></script>
<script>
wan_route_x = '<% nvram_get_x("IPConnection", "wan_route_x"); %>';
wan_nat_x = '<% nvram_get_x("IPConnection", "wan_nat_x"); %>';
wan_proto = '<% nvram_get_x("Layer3Forwarding", "wan_proto"); %>';

<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>]; // [[MAC, associated, authorized], ...]

var modem = '<% nvram_get_x("General", "Dev3G"); %>';
var country = '<% nvram_get_x("General", "modem_country"); %>';
var isp = '<% nvram_get_x("General", "modem_isp"); %>';

var apn = '<% nvram_get_x("General", "modem_apn"); %>';
var dialnum = '<% nvram_get_x("General", "modem_dialnum"); %>';
var user = '<% nvram_get_x("General", "modem_user"); %>';
var pass = '<% nvram_get_x("General", "modem_pass"); %>';

var modemlist = new Array();
var countrylist = new Array();
var isplist = new Array();

var apnlist = new Array();
var daillist = new Array();
var userlist = new Array();
var passlist = new Array();

function initial(){
	show_banner(1);
	show_menu(5, 4, 4);
	show_footer();

	switch_modem_mode(document.form.modem_enable.value);
	gen_list(document.form.modem_enable.value);
	show_ISP_list();
	show_APN_list();
	
	enable_auto_hint(21, 7);
}

function show_modem_list(mode){
	if(mode == "4")
		show_4G_modem_list();
	else
		show_3G_modem_list();
}

function show_3G_modem_list(){
	modemlist = new Array(
			"AUTO"
			, "ASUS-T500"
			, "BandLuxe-C120"
			, "BandLuxe-C170"
			, "BandLuxe-C339"
			, "Huawei-E1550"
			, "Huawei-E160G"
			, "Huawei-E161"
			, "Huawei-E169"
			, "Huawei-E176"
			, "Huawei-E220"
			, "Huawei-K3520"
			, "Huawei-ET128"
			, "Huawei-E1800"
			, "Huawei-K4505"
			, "Huawei-E172"
			, "Huawei-E372"
			, "Huawei-E122"
			, "Huawei-EC306"
			, "Huawei-E160E"
			, "Huawei-E1552"
			, "Huawei-E173"
			, "Huawei-E1823"
			, "Huawei-E1762"
			, "Huawei-E4505"
			, "Huawei-E1750C"
			, "Huawei-E1752Cu"
			//, "MU-Q101"
			, "Sierra-U598"
			, "Alcatel-X200"
			, "Alcatel-Oune-touch-X220S"
			, "AnyData-ADU-510A"
			, "AnyData-ADU-500A"
			, "Onda-MT833UP"
			, "Onda-MW833UP"
			, "ZTE-AC5710"
			, "ZTE-MU351"
			, "ZTE-MF100"
			, "ZTE-MF636"
			, "ZTE-MF622"
			, "ZTE-MF626"
			, "ZTE-MF632"
			, "ZTE-MF112"
			, "ZTE-MFK3570-Z"
			, "CS15"
			, "CS17"
			, "ICON401"
			);

	free_options($("shown_modems"));
	for(var i = 0; i < modemlist.length; i++){
		$("shown_modems").options[i] = new Option(modemlist[i], modemlist[i]);
		if(modemlist[i] == modem)
			$("shown_modems").options[i].selected = "1";
	}
}

function show_4G_modem_list(){
	modemlist = new Array(
			"AUTO"
			, "Samsung U200"
			//, "Beceem BCMS250"
			);

	free_options($("shown_modems"));
	for(var i = 0; i < modemlist.length; i++){
		$("shown_modems").options[i] = new Option(modemlist[i], modemlist[i]);
		if(modemlist[i] == modem)
			$("shown_modems").options[i].selected = "1";
	}
}

function switch_modem_mode(mode){
	//if(mode != "0" && $("hsdpa_hint").style.display == "none")
	//	alert("<#HSDPA_LANtoWAN#>");

	show_modem_list(mode);

	if(mode == "1"){ // WCDMA
		document.form.Dev3G.disabled = false;
		document.form.modem_country.disabled = false;
		document.form.modem_isp.disabled = false;
		document.form.modem_apn.disabled = false;
		document.form.wan_3g_pin.disabled = false;
		document.form.modem_dialnum.disabled = false;
		document.form.modem_user.disabled = false;
		document.form.modem_pass.disabled = false;
		document.form.modem_ttlsid.disabled = true;
		//$("hsdpa_hint").style.display = "";
	}
	else if(mode == "2"){ // CDMA2000
		document.form.Dev3G.disabled = false;
		document.form.modem_country.disabled = false;
		document.form.modem_isp.disabled = false;
		document.form.modem_apn.disabled = false;
		document.form.wan_3g_pin.disabled = false;
		document.form.modem_dialnum.disabled = false;
		document.form.modem_user.disabled = false;
		document.form.modem_pass.disabled = false;
		document.form.modem_ttlsid.disabled = true;
		//$("hsdpa_hint").style.display = "";
	}
	else if(mode == "3"){ // TD-SCDMA
		document.form.Dev3G.disabled = false;
		document.form.modem_country.disabled = false;
		document.form.modem_isp.disabled = false;
		document.form.modem_apn.disabled = false;
		document.form.wan_3g_pin.disabled = false;
		document.form.modem_dialnum.disabled = false;
		document.form.modem_user.disabled = false;
		document.form.modem_pass.disabled = false;
		document.form.modem_ttlsid.disabled = true;
		//$("hsdpa_hint").style.display = "";
	}
	else if(mode == "4"){
		document.form.Dev3G.disabled = false;
		document.form.modem_country.disabled = false;
		document.form.modem_isp.disabled = false;
		document.form.modem_apn.disabled = true;
		document.form.wan_3g_pin.disabled = true;
		document.form.modem_dialnum.disabled = true;
		document.form.modem_user.disabled = false;
		document.form.modem_pass.disabled = false;
		document.form.modem_ttlsid.disabled = false;
		//$("hsdpa_hint").style.display = "";
	}
	else{
		document.form.Dev3G.disabled = true;
		document.form.modem_country.disabled = true;
		document.form.modem_isp.disabled = true;
		document.form.modem_apn.disabled = true;
		document.form.wan_3g_pin.disabled = true;
		document.form.modem_dialnum.disabled = true;
		document.form.modem_user.disabled = true;
		document.form.modem_pass.disabled = true;
		document.form.modem_ttlsid.disabled = true;
		//$("hsdpa_hint").style.display = "none";
	}

	gen_country_list(mode);
}

function gen_country_list(mode){
	if(mode == "1"){
		show_wcdma_country_list();
	}
	else if(mode == "2"){
		show_cdma2000_country_list();
	}
	else if(mode == "3"){
		show_tdscdma_country_list();
	}
	else if(mode == "4"){
		show_4G_country_list();
	}
}

function gen_list(mode){
	if(mode == "1"){
		gen_wcdma_list();
	}
	else if(mode == "2"){
		gen_cdma2000_list();
	}
	else if(mode == "3"){
		gen_tdscdma_list();
	}
	else if(mode == "4"){
		gen_4G_list();
	}
}

function show_ISP_list(){
	free_options($("modem_isp"));
	$("modem_isp").options.length = isplist.length;

	for(var i = 0; i < isplist.length; i++){
		$("modem_isp").options[i] = new Option(isplist[i], isplist[i]);
		if(isplist[i] == isp)
			$("modem_isp").options[i].selected = "1";
	}
}

function show_APN_list(){
	var ISPlist = $("modem_isp").value;

	if(document.form.modem_enable.value == "1"
			|| document.form.modem_enable.value == "2"
			|| document.form.modem_enable.value == "3"
			){
		if(ISPlist == isp
				&& (apn != "" || dialnum != "" || user != "" || pass != "")){
			$("modem_apn").value = apn;
			$("modem_dialnum").value = dialnum;
			$("modem_user").value = user;
			$("modem_pass").value = pass;
		}
		else{
			for(var i = 0; i < isplist.length; i++){
				if(isplist[i] == ISPlist){
					$("modem_apn").value = apnlist[i];
					$("modem_dialnum").value = daillist[i];
					$("modem_user").value = userlist[i];
					$("modem_pass").value = passlist[i];
				}
			}
		}
	}
	else if(document.form.modem_enable.value == "4"){
		$("modem_apn").value = "";
		$("modem_dialnum").value = "";

		if(ISPlist == isp
				&& (user != "" || pass != "")){
			$("modem_user").value = user;
			$("modem_pass").value = pass;
		}
		else{
			for(var i = 0; i < isplist.length; i++){
				if(isplist[i] == ISPlist){
					$("modem_user").value = userlist[i];
					$("modem_pass").value = passlist[i];
				}
			}
		}
	}
}

function applyRule(){
	var mode = document.form.modem_enable.value;

	if(document.form.modem_enable.value != "0"){
		//if(confirm("<#HSDPA_enable_confirm#>")){
			showLoading(); 

			document.form.action_mode.value = " Apply ";
			document.form.current_page.value = "/index.asp";
			document.form.next_page.value = "";

			document.form.submit();
		//}
	}
	else{
		showLoading(); 

		document.form.action_mode.value = " Apply ";
		document.form.current_page.value = "/as.asp";
		document.form.next_page.value = "";

		document.form.submit();
	}
}

function done_validating(action){
	refreshpage();
}
</script>
</head>

<body onload="initial();" onunLoad="disable_auto_hint(21, 7);return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get_f("general.log", "productid"); %>">

<input type="hidden" name="current_page" value="Advanced_Modem_others.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="sid_list" value="General;Layer3Forwarding;">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get_x("LANGUAGE", "preferred_lang"); %>">
<input type="hidden" name="wl_ssid2" value="<% nvram_char_to_ascii("WLANConfig11b", "wl_ssid"); %>">
<input type="hidden" name="firmver" value="<% nvram_get_x("", "firmver"); %>">

<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
        <td width="23">&nbsp;</td>
        
        <!--=====Beginning of Main Menu=====-->
        <td valign="top" width="202">
          <div id="mainMenu"></div>
          <div id="subMenu"></div>
        </td>
        
    <td valign="top">
        <div id="tabMenu" class="submenuBlock"></div>
                <br />
                <!--===================================Beginning of Main Content===========================================-->
<table width="98%" border="0" align="center" cellpadding="0" cellspacing="0">
        <tr>
                <td align="left" valign="top" >
                
<table width="90%" border="0" align="center" cellpadding="5" cellspacing="0" class="FormTitle" table>
        <thead>
        <tr>
                <td><#menu5_4_4#></td>
        </tr>
        </thead>
        <tbody>
          <tr>
            <td bgcolor="#FFFFFF"><#HSDPAConfig_hsdpa_mode_itemdesc#></td>
          </tr>
        </tbody>
        <tr>
                <td bgcolor="#FFFFFF">
	<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">

                                <tr>
                                <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,9);"><#HSDPAConfig_Country_itemname#></a></th>
                                <td>
                                	<select name="modem_country" id="isp_countrys" class="input" onfocus="parent.showHelpofDrSurf(21,9);" onchange="gen_list(document.form.modem_enable.value);show_ISP_list();show_APN_list();"></select>
                                </td>
                                </tr>
                                
                                <tr>
                                <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,8);"><#HSDPAConfig_ISP_itemname#></a></th>
                                <td>
                                	<select name="modem_isp" id="modem_isp" class="input" onClick="openHint(21,8);" onchange="show_APN_list()"></select>
                                </td>
                                </tr>

		<tr>
			<th width="40%">
				<a class="hintstyle" href="javascript:openHint(21,1);"><#HSDPAConfig_hsdpa_enable_itemname#></a>
			</th>
			<td>
				<select name="modem_enable" class="input" onClick="openHint(21,1);" onchange="switch_modem_mode(this.value);gen_list(this.value);show_ISP_list();show_APN_list();">
					<option value="0" <% nvram_match_x("General", "modem_enable", "0", "selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
					<option value="1" <% nvram_match_x("General", "modem_enable", "1", "selected"); %>>WCDMA (UMTS)</option>
					<option value="2" <% nvram_match_x("General", "modem_enable", "2", "selected"); %>>CDMA2000 (EVDO)</option>
					<option value="3" <% nvram_match_x("General", "modem_enable", "3", "selected"); %>>TD-SCDMA</option>
					<option value="4" <% nvram_match_x("General", "modem_enable", "4", "selected"); %>>WiMAX</option>
				</select>
				
				<br/><span id="hsdpa_hint" style="display:none;"><#HSDPAConfig_hsdpa_enable_hint1#></span>
			</td>
		</tr>
                                
	<tr>
		<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,13);"><#HSDPAConfig_USBAdapter_itemname#></a></th>
		<td>
			<select name="Dev3G" id="shown_modems" class="input" onClick="openHint(21,13);" disabled="disabled"></select>
		</td>
	</tr>
                                	
                                <tr>
                                <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,3);"><#HSDPAConfig_private_apn_itemname#></a></th>
                                <td>
                                	<input id="modem_apn" name="modem_apn" class="input" onClick="openHint(21,3);" type="text" value=""/>
                                </td>
                                </tr>

                                <tr>
                                <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,2);"><#HSDPAConfig_PIN_itemname#></a></th>
                                <td>
                                	<input id="wan_3g_pin" name="wan_3g_pin" class="input" onClick="openHint(21,2);" type="password" maxLength="8" value="<% nvram_get_x("", "wan_3g_pin"); %>"/>
                                </td>
                                </tr>
                                
                                <tr>
                                <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,10);"><#HSDPAConfig_DialNum_itemname#></a></th>
                                <td>
                                	<input id="modem_dialnum" name="modem_dialnum" class="input" onClick="openHint(21,10);" type="text" value=""/>
                                </td>
                                </tr>
                                
                                <tr>
                                <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,11);"><#HSDPAConfig_Username_itemname#></a></th>
                                <td>
                                	<input id="modem_user" name="modem_user" class="input" onClick="openHint(21,11);" type="text" value="<% nvram_get_x("", "modem_user"); %>"/>
                                </td>
                                </tr>
                                
                                <tr>
                                <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(21,12);"><#AiDisk_Password#></a></th>
                                <td>
                                	<input id="modem_pass" name="modem_pass" class="input" onClick="openHint(21,12);" type="password" value="<% nvram_get_x("", "modem_pass"); %>"/>
                                </td>
                                </tr>
                                <tr>
                                <th>E-mail</th>
                                <td>
                                	<input id="modem_ttlsid" name="modem_ttlsid" class="input" value="<% nvram_get_x("", "modem_ttlsid"); %>"/>
                                </td>
                                </tr>
                                <tr align="right">
                                        <td colspan="2">
                                                <input type="button" class="button" value="<#CTL_apply#>" onclick="applyRule();">
                                        </td>
                                </tr>
                        </table>
                </td>
        </tr>
</table>                

                </td>
</form>

                                        <!--==============Beginning of hint content=============-->
                                        <td id="help_td" style="width:15px;" valign="top">
                                                <form name="hint_form"></form>
                                                <div id="helpicon" onClick="openHint(0,0);" title="<#Help_button_default_hint#>"><img src="images/help.gif" /></div>
                                                <div id="hintofPM" style="display:none;">
                                                        <table width="100%" cellpadding="0" cellspacing="1" class="Help" bgcolor="#999999">
                                                                <thead>
                                                                <tr>
                                                                        <td>
                                                                                <div id="helpname" class="AiHintTitle"></div>
                                                                                <a href="javascript:closeHint();">
                                                                                        <img src="images/button-close.gif" class="closebutton">
                                                                                </a>
                                                                        </td>
                                                                </tr>
                                                                </thead>
                                                                
                                                                <tr>
                                                                        <td valign="top">
                                                                                <div class="hint_body2" id="hint_body"></div>
                                                                                <iframe id="statusframe" name="statusframe" class="statusframe" src="" frameborder="0"></iframe>
                                                                        </td>
                                                                </tr>
                                                        </table>
                                                </div>
                                        </td>
                                        <!--==============Ending of hint content=============-->
                                        
                                </tr>
                        </table>                                
                </td>
                
    <td width="10" align="center" valign="top">&nbsp;</td>
        </tr>
</table>

<div id="footer"></div>
</body>
</html>
