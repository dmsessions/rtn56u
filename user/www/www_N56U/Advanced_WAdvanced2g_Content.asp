﻿<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>ASUS Wireless Router <#Web_Title#> - <#menu5_1_6#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general_2g.js"></script>
<script type="text/javascript" src="/help_2g.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script>
wan_route_x = '<% nvram_get_x("IPConnection", "wan_route_x"); %>';
wan_nat_x = '<% nvram_get_x("IPConnection", "wan_nat_x"); %>';
wan_proto = '<% nvram_get_x("Layer3Forwarding",  "wan_proto"); %>';

<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var check_hwnat = '<% check_hwnat(); %>';
var hwnat = '<% nvram_get_x("",  "hwnat"); %>';

function initial(){

	show_banner(1);
	
	if(sw_mode == "2"){
		show_menu(5,1,1);
		disableAdvFn(17);
	}
	else if(sw_mode == "3")
		show_menu(5,1,5);
	else
		show_menu(5,1,6);
		
	show_footer();
	
	enable_auto_hint(3, 20);

	load_body();
	$("rt_rate").style.display = "none";
	
	if(document.form.rt_gmode.value == "3")
		$("rt_HT_OpMode").disabled = false;
	else{
		$("rt_HT_OpMode").value = "0";
		$("rt_HT_OpMode").disabled = true;
	}

	if(document.form.rt_gmode.value == "3" || document.form.rt_gmode.value == "2"){
		$("rt_wme").value = "1";
		$("rt_wme").disabled = true;
	}
	else
		$("rt_wme").disabled = false;
		
	/*if(document.form.rt_mrate.value != "0" && hwnat == "1" && sw_mode == "1")
		alert("<#BasicConfig_HWNAT_alert#>");*/
}

function applyRule(){
	
	if(validForm()){
		/*if(document.form.hwnat.value == 1 && document.form.rt_mrate.value != "0" && sw_mode == "1"){
			if(confirm("<#BasicConfig_HWNAT_suggestion#>"))
				document.form.hwnat.value = "0";
		}*/
		updateDateTime(document.form.current_page.value);
		
		showLoading();
		
		document.form.action_mode.value = " Apply ";
		document.form.current_page.value = "/as.asp";
		document.form.next_page.value = "";
		
		document.form.submit();
	}
}

function validForm(){
	if(sw_mode != "2"){
		if(!validate_range(document.form.rt_frag, 256, 2346)
				|| !validate_range(document.form.rt_rts, 0, 2347)
				|| !validate_range(document.form.rt_dtim, 1, 255)
				|| !validate_range(document.form.rt_bcn, 20, 1024)
				)
			return false;
	}
	
if(document.form.rt_radio_x[0].checked){
	if(!validate_timerange(document.form.rt_radio_time_x_starthour, 0)
			|| !validate_timerange(document.form.rt_radio_time_x_startmin, 1)
			|| !validate_timerange(document.form.rt_radio_time_x_endhour, 2)
			|| !validate_timerange(document.form.rt_radio_time_x_endmin, 3)
			){	return false;}

	var starttime = eval(document.form.rt_radio_time_x_starthour.value + document.form.rt_radio_time_x_startmin.value);
	var endtime = eval(document.form.rt_radio_time_x_endhour.value + document.form.rt_radio_time_x_endmin.value);				
	
	if(starttime > endtime){
		alert("<#FirewallConfig_URLActiveTime_itemhint#>");
			document.form.rt_radio_time_x_starthour.focus();
			document.form.rt_radio_time_x_starthour.select;
		return false;  
	}
	if(starttime == endtime){
		alert("<#FirewallConfig_URLActiveTime_itemhint2#>");
			document.form.rt_radio_time_x_starthour.focus();
			document.form.rt_radio_time_x_starthour.select;
		return false;  
	}
}		
		
	//alert(document.form.rt_radio_x[0].checked+","+document.form.rt_radio_x[1].checked+","+document.form.rt_radio_date_x_Sun.checked+","+document.form.rt_radio_date_x_Mon.checked+","+document.form.rt_radio_date_x_Tue.checked+","+document.form.rt_radio_date_x_Wed.checked+","+document.form.rt_radio_date_x_Thu.checked+","+document.form.rt_radio_date_x_Fri.checked+","+document.form.rt_radio_date_x_Sat.checked);
	if((document.form.rt_radio_x[0].checked ==true) 
		&& (document.form.rt_radio_date_x_Sun.checked ==false)
		&& (document.form.rt_radio_date_x_Mon.checked ==false)
		&& (document.form.rt_radio_date_x_Tue.checked ==false)
		&& (document.form.rt_radio_date_x_Wed.checked ==false)
		&& (document.form.rt_radio_date_x_Thu.checked ==false)
		&& (document.form.rt_radio_date_x_Fri.checked ==false)
		&& (document.form.rt_radio_date_x_Sat.checked ==false)){
			alert("<#WLANConfig11b_x_RadioEnableDate_itemname#><#JS_fieldblank#>");
			document.form.rt_radio_x[0].checked=false;
			document.form.rt_radio_x[1].checked=true;
			return false;			
	}
	
	return true;
}

function done_validating(action){
	refreshpage();
}

function disableAdvFn(row){
	for(var i=row; i>=3; i--){
		$("WAdvTable").deleteRow(i);
	}
}
</script>
</head>

<body onload="initial();" onunLoad="disable_auto_hint(3, 16);return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get_f("general.log","productid"); %>">
<input type="hidden" name="wan_route_x" value="<% nvram_get_x("IPConnection","wan_route_x"); %>">
<input type="hidden" name="wan_nat_x" value="<% nvram_get_x("IPConnection","wan_nat_x"); %>">

<input type="hidden" name="rt_gmode" value="<% nvram_get_x("WLANConfig11b","rt_gmode"); %>">
<input type="hidden" name="rt_gmode_protection_x" value="<% nvram_get_x("WLANConfig11b","rt_gmode_protection_x"); %>">

<input type="hidden" name="current_page" value="Advanced_WAdvanced_Content.asp">
<input type="hidden" name="next_page" value="SaveRestart.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="sid_list" value="WLANAuthentication11a;WLANConfig11b;LANHostConfig;PrinterStatus;">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get_x("LANGUAGE", "preferred_lang"); %>">
<input type="hidden" name="rt_ssid2" value="<% nvram_char_to_ascii("WLANConfig11b", "rt_ssid"); %>">
<input type="hidden" name="firmver" value="<% nvram_get_x("",  "firmver"); %>">

<input type="hidden" name="rt_radio_date_x" value="<% nvram_get_x("WLANConfig11b","rt_radio_date_x"); %>">
<input type="hidden" name="rt_radio_time_x" value="<% nvram_get_x("WLANConfig11b","rt_radio_time_x"); %>">
<input type="hidden" name="hwnat" value="<% nvram_get_x("PrinterStatus","hwnat"); %>">

<table class="content" align="center" cellpadding="0" cellspacing="0">
	<tr>
		<td width="23">&nbsp;</td>
		
		<td valign="top" width="202">				
		<div  id="mainMenu"></div>	
		<div  id="subMenu"></div>		
		</td>				
		
    <td valign="top">
	<div id="tabMenu" class="submenuBlock"></div>
		<br />
		<!--===================================Beginning of Main Content===========================================-->
<table width="98%" border="0" align="center" cellpadding="0" cellspacing="0">
	<tr>
		<td valign="top" >
		
<table width="98%" border="0" align="center" cellpadding="5" cellspacing="0" class="FormTitle" table>
	<thead>
	<tr>
		<td><#menu5_1#> - <#menu5_1_6#> (2.4GHz)</td>
	</tr>
	</thead>
	<tbody>
	<tr>
		<td bgcolor="#FFFFFF"><#WLANConfig11b_display5_sectiondesc#></td>
	</tr>
	</tbody>	
	<tr>
	  <td bgcolor="#FFFFFF">
		<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable" id="WAdvTable">
			<tr>
			  <th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 1);"><#WLANConfig11b_x_RadioEnable_itemname#></a></th>
			  <td>
			  	<input type="radio" value="1" name="rt_radio_x" class="input" onClick="return change_common_radio(this, 'WLANConfig11b', 'rt_radio_x', '1')" <% nvram_match_x("WLANConfig11b","rt_radio_x", "1", "checked"); %>><#checkbox_Yes#>
			    <input type="radio" value="0" name="rt_radio_x" class="input" onClick="return change_common_radio(this, 'WLANConfig11b', 'rt_radio_x', '0')" <% nvram_match_x("WLANConfig11b","rt_radio_x", "0", "checked"); %>><#checkbox_No#>
			  </td>
			</tr>
			<tr>
			  <th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 2);"><#WLANConfig11b_x_RadioEnableDate_itemname#></a></th>
			  <td>
				<input type="checkbox" class="input" name="rt_radio_date_x_Sun" onChange="return changeDate();">Sun
				<input type="checkbox" class="input" name="rt_radio_date_x_Mon" onChange="return changeDate();">Mon
				<input type="checkbox" class="input" name="rt_radio_date_x_Tue" onChange="return changeDate();">Tue
				<input type="checkbox" class="input" name="rt_radio_date_x_Wed" onChange="return changeDate();">Wed
				<input type="checkbox" class="input" name="rt_radio_date_x_Thu" onChange="return changeDate();">Thu
				<input type="checkbox" class="input" name="rt_radio_date_x_Fri" onChange="return changeDate();">Fri
				<input type="checkbox" class="input" name="rt_radio_date_x_Sat" onChange="return changeDate();">Sat			  
			  </td>
			</tr>
			<tr>
			  <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(3, 3);"><#WLANConfig11b_x_RadioEnableTime_itemname#></a></th>
			  <td>
			  	<input type="text" maxlength="2" class="input" size="2" name="rt_radio_time_x_starthour" onKeyPress="return is_number(this)">:
					<input type="text" maxlength="2" class="input" size="2" name="rt_radio_time_x_startmin" onKeyPress="return is_number(this)">-
					<input type="text" maxlength="2" class="input" size="2" name="rt_radio_time_x_endhour" onKeyPress="return is_number(this)">:
					<input type="text" maxlength="2" class="input" size="2" name="rt_radio_time_x_endmin" onKeyPress="return is_number(this)">
				</td>
			</tr>
			
			<tr>
			  <th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 5);"><#WLANConfig11b_x_IsolateAP_itemname#></a></th>
			  <td>
				<input type="radio" value="1" name="rt_ap_isolate" class="input" onClick="return change_common_radio(this, 'WLANConfig11b', 'rt_ap_isolate', '1')" <% nvram_match_x("WLANConfig11b","rt_ap_isolate", "1", "checked"); %>><#checkbox_Yes#>
				<input type="radio" value="0" name="rt_ap_isolate" class="input" onClick="return change_common_radio(this, 'WLANConfig11b', 'rt_ap_isolate', '0')" <% nvram_match_x("WLANConfig11b","rt_ap_isolate", "0", "checked"); %>><#checkbox_No#>
			  </td>
			</tr>
			
			<tr id="rt_rate">
			  <th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 6);"><#WLANConfig11b_DataRateAll_itemname#></a></th>
			  <td>
				<select name="rt_rate" class="input" onChange="return change_common(this, 'WLANConfig11b', 'rt_rate')">
				  <option value="0" <% nvram_match_x("WLANConfig11b","rt_rate", "0","selected"); %>>Auto</option>
				  <option value="1000000" <% nvram_match_x("WLANConfig11b","rt_rate", "1000000","selected"); %>>1</option>
				  <option value="2000000" <% nvram_match_x("WLANConfig11b","rt_rate", "2000000","selected"); %>>2</option>
				  <option value="5500000" <% nvram_match_x("WLANConfig11b","rt_rate", "5500000","selected"); %>>5.5</option>
				  <option value="6000000" <% nvram_match_x("WLANConfig11b","rt_rate", "6000000","selected"); %>>6</option>
				  <option value="9000000" <% nvram_match_x("WLANConfig11b","rt_rate", "9000000","selected"); %>>9</option>
				  <option value="11000000" <% nvram_match_x("WLANConfig11b","rt_rate", "11000000","selected"); %>>11</option>
				  <option value="12000000" <% nvram_match_x("WLANConfig11b","rt_rate", "12000000","selected"); %>>12</option>
				  <option value="18000000" <% nvram_match_x("WLANConfig11b","rt_rate", "18000000","selected"); %>>18</option>
				  <option value="24000000" <% nvram_match_x("WLANConfig11b","rt_rate", "24000000","selected"); %>>24</option>
				  <option value="36000000" <% nvram_match_x("WLANConfig11b","rt_rate", "36000000","selected"); %>>36</option>
				  <option value="48000000" <% nvram_match_x("WLANConfig11b","rt_rate", "48000000","selected"); %>>48</option>
				  <option value="54000000" <% nvram_match_x("WLANConfig11b","rt_rate", "54000000","selected"); %>>54</option>
				</select>
			  </td>
			</tr>
			
			<!-- 2008.03 James. patch for Oleg's patch. { -->
			<tr>
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 7);"><#WLANConfig11b_MultiRateAll_itemname#></a></th>
				<td>
					<select name="rt_mrate" class="input" onClick="openHint(3, 7);" onChange="return change_common(this, 'WLANConfig11b', 'rt_mrate')">
						<option value="0" <% nvram_match_x("WLANConfig11b", "rt_mrate", "0", "selected"); %>>Disable</option>
						<!--option value="1" <% nvram_match_x("WLANConfig11b", "rt_mrate", "1", "selected"); %>>1</option>
						<option value="2" <% nvram_match_x("WLANConfig11b", "rt_mrate", "2", "selected"); %>>2</option>
						<option value="3" <% nvram_match_x("WLANConfig11b", "rt_mrate", "3", "selected"); %>>5.5</option>
						<option value="4" <% nvram_match_x("WLANConfig11b", "rt_mrate", "4", "selected"); %>>6</option>
						<option value="5" <% nvram_match_x("WLANConfig11b", "rt_mrate", "5", "selected"); %>>9</option>
						<option value="6" <% nvram_match_x("WLANConfig11b", "rt_mrate", "6", "selected"); %>>11</option>
						<option value="7" <% nvram_match_x("WLANConfig11b", "rt_mrate", "7", "selected"); %>>12</option>
						<option value="8" <% nvram_match_x("WLANConfig11b", "rt_mrate", "8", "selected"); %>>18</option>
						<option value="9" <% nvram_match_x("WLANConfig11b", "rt_mrate", "9", "selected"); %>>24</option>
						<option value="10" <% nvram_match_x("WLANConfig11b", "rt_mrate", "10", "selected"); %>>36</option>
						<option value="11" <% nvram_match_x("WLANConfig11b", "rt_mrate", "11", "selected"); %>>48</option>
						<option value="12" <% nvram_match_x("WLANConfig11b", "rt_mrate", "12", "selected"); %>>54</option-->
						<option value="13" <% nvram_match_x("WLANConfig11b", "rt_mrate", "13", "selected"); %>>Auto</option>
					</select>
				</td>
			</tr>
			<!-- 2008.03 James. patch for Oleg's patch. } -->
			
			<tr>
			  <th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 8);"><#WLANConfig11b_DataRate_itemname#></a></th>
			  <td>
			  	<select name="rt_rateset" class="input" onChange="return change_common(this, 'WLANConfig11b', 'rt_rateset')">
				  <option value="default" <% nvram_match_x("WLANConfig11b","rt_rateset", "default","selected"); %>>Default</option>
				  <option value="all" <% nvram_match_x("WLANConfig11b","rt_rateset", "all","selected"); %>>All</option>
				  <option value="12" <% nvram_match_x("WLANConfig11b","rt_rateset", "12","selected"); %>>1, 2 Mbps</option>
				</select></td>
			</tr>
			<tr>
			  <th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 9);"><#WLANConfig11b_x_Frag_itemname#></a></th>
			  	<td>
			  		<input type="text" maxlength="5" size="5" name="rt_frag" class="input" value="<% nvram_get_x("WLANConfig11b", "rt_frag"); %>" onKeyPress="return is_number(this)" onChange="page_changed()" onBlur="validate_range(this, 256, 2346)">
				</td>
			</tr>
			<tr>
			  <th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 10);"><#WLANConfig11b_x_RTS_itemname#></a></th>
			  	<td>
			  		<input type="text" maxlength="5" size="5" name="rt_rts" class="input" value="<% nvram_get_x("WLANConfig11b", "rt_rts"); %>" onKeyPress="return is_number(this)">
			  	</td>
			</tr>
			<tr>
			  <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(3, 11);"><#WLANConfig11b_x_DTIM_itemname#></a></th>
				<td>
			  		<input type="text" maxlength="5" size="5" name="rt_dtim" class="input" value="<% nvram_get_x("WLANConfig11b", "rt_dtim"); %>" onKeyPress="return is_number(this)"  onBlur="validate_range(this, 1, 255)">
				</td>
			  
			</tr>
			<tr>
			  <th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 12);"><#WLANConfig11b_x_Beacon_itemname#></a></th>
				<td>
					<input type="text" maxlength="5" size="5" name="rt_bcn" class="input" value="<% nvram_get_x("WLANConfig11b", "rt_bcn"); %>" onKeyPress="return is_number(this)" onBlur="validate_range(this, 20, 1024)">
				</td>
			</tr>
			<tr>
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 13);"><#WLANConfig11b_x_TxBurst_itemname#></a></th>
				<td>
					<select name="rt_TxBurst" class="input" onChange="return change_common(this, 'WLANConfig11b', 'rt_TxBurst')">
						<option value="0" <% nvram_match_x("WLANConfig11b","rt_TxBurst", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
						<option value="1" <% nvram_match_x("WLANConfig11b","rt_TxBurst", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
					</select>
				</td>
			</tr>			
			<tr>
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 16);"><#WLANConfig11b_x_PktAggregate_itemname#></a></th>
				<td>
					<select name="rt_PktAggregate" class="input" onChange="return change_common(this, 'WLANConfig11b', 'rt_PktAggregate')">
						<option value="0" <% nvram_match_x("WLANConfig11b","rt_PktAggregate", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
						<option value="1" <% nvram_match_x("WLANConfig11b","rt_PktAggregate", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
					</select>
				</td>
			</tr>
			<!--Greenfield by Lock Add in 2008.10.01 -->
			<tr>
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3, 19);"><#WLANConfig11b_x_HT_OpMode_itemname#></a></th>
				<td>
					<select class="input" id="rt_HT_OpMode" name="rt_HT_OpMode" onChange="return change_common(this, 'WLANConfig11b', 'rt_HT_OpMode')">
						<option value="0" <% nvram_match_x("WLANConfig11b","rt_HT_OpMode", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
						<option value="1" <% nvram_match_x("WLANConfig11b","rt_HT_OpMode", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
					</select>
				</td>
			</tr>
			<!--Greenfield by Lock Add in 2008.10.01 -->
			<tr>
			  <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(3, 14);"><#WLANConfig11b_x_WMM_itemname#></a></th>
			  <td>
				<select name="rt_wme" id="rt_wme" class="input" onChange="return change_common(this, 'WLANConfig11b', 'rt_wme')">
			  	  <option value="0" <% nvram_match_x("WLANConfig11b", "rt_wme", "0", "selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
			  	  <option value="1" <% nvram_match_x("WLANConfig11b", "rt_wme", "1", "selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
				</select>
			  </td>
			</tr>
			<tr>
			  <th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,15);"><#WLANConfig11b_x_NOACK_itemname#></a></th>
			  <td>
				<select name="rt_wme_no_ack" id="rt_wme_no_ack" class="input" onChange="return change_common(this, 'WLANConfig11b', 'rt_wme_no_ack')">
			  	  <option value="off" <% nvram_match_x("WLANConfig11b","rt_wme_no_ack", "off","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
			  	  <option value="on" <% nvram_match_x("WLANConfig11b","rt_wme_no_ack", "on","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
				</select>
			  </td>
			</tr>
			<tr>
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,17);"><#WLANConfig11b_x_APSD_itemname#></a></th>
				<td>
                  <select name="rt_APSDCapable" class="input" onchange="return change_common(this, 'WLANConfig11b', 'rt_APSDCapable')">
                    <option value="0" <% nvram_match_x("WLANConfig11b","rt_APSDCapable", "0","selected"); %> ><#WLANConfig11b_WirelessCtrl_buttonname#></option>
                    <option value="1" <% nvram_match_x("WLANConfig11b","rt_APSDCapable", "1","selected"); %> ><#WLANConfig11b_WirelessCtrl_button1name#></option>
                  </select>
				</td>
			</tr>
			<tr>
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(3,18);"><#WLANConfig11b_x_DLS_itemname#></a></th>
				<td>
					<select name="rt_DLSCapable" class="input" onChange="return change_common(this, 'WLANConfig11b', 'rt_DLSCapable')">
						<option value="0" <% nvram_match_x("WLANConfig11b","rt_DLSCapable", "0","selected"); %>><#WLANConfig11b_WirelessCtrl_buttonname#></option>
						<option value="1" <% nvram_match_x("WLANConfig11b","rt_DLSCapable", "1","selected"); %>><#WLANConfig11b_WirelessCtrl_button1name#></option>
					</select>
				</td>
			</tr>
			<tr align="right">
				<td colspan="2">
					<input type="button" class="button5" value="<#GO_5G#>" onclick="location.href='Advanced_WAdvanced_Content.asp';">    
					<input class="button" onclick="applyRule();" type="button" value="<#CTL_apply#>"/></td>
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
	  <div id="helpicon" onClick="openHint(0, 0);" title="<#Help_button_default_hint#>">
		<img src="images/help.gif">
	  </div>
	  
	  <div id="hintofPM" style="display:none;">
		<table width="100%" cellpadding="0" cellspacing="1" class="Help" bgcolor="#999999">
		  <thead>
		  <tr>
			<td>
			  <div id="helpname" class="AiHintTitle"></div>
			  <a href="javascript:closeHint();"><img src="images/button-close.gif" class="closebutton" /></a>
			</td>
		  </tr>
		  </thead>
		  
		  <tbody>
		  <tr>
			<td valign="top">
			  <div id="hint_body" class="hint_body2"></div>
			  <iframe id="statusframe" name="statusframe" class="statusframe" src="" frameborder="0"></iframe>
			</td>
		  </tr>
		  </tbody>
		</table>
	  </div>
	</td>
	<!--==============Ending of hint content=============-->
        </tr>
      </table>				
		<!--===================================Ending of Main Content===========================================-->		
	</td>
		
    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>
</body>
</html>
