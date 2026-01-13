#pragma once

const char MAIN_PAGE[] =
R"=====(
<!DOCTYPE html>
<html>
<head>
<meta charset='utf-8'>
<title>Web-Switch</title>
<style>
/* your styling untouched */
body {
  background-image: url('https://www.ruag.ch/sites/default/files/styles/xxxl/public/2020-04/RUA_Unternehmensbilder_Schweizerisch_01_0.webp?itok=YsleXNGz');
  background-repeat: no-repeat;
  background-position: center 40%;
  background-size: auto;
  color:white;
  font-family:'Segoe UI', Arial, sans-serif;
  text-align:center;
  margin:0;
  padding:20px;
}
h1, h2, h3 { color: #FF0000; }
p, label { color: black; }
.led { display:inline-block;width:20px;height:20px;border-radius:50%;
  background:#555;box-shadow:inset 0 1px 2px rgba(255,255,255,0.4),0 1px 3px rgba(0,0,0,0.5);
  position:relative;margin-right:10px;vertical-align:middle;transition:all .2s;
}
.led.on {
  background:radial-gradient(circle at 30% 30%, #00c0ff, #007bb5);
  box-shadow:inset 0 1px 2px rgba(255,255,255,0.4),
             0 0 8px rgba(0,192,255,0.8),
             0 2px 4px rgba(0,0,0,0.5);
}
.led.off { background-color: gray; }
/* rest of CSS… */
</style>
</head>

<body>
<div id='header'>
  <span id='headerText'>Web-Switch Walker Josef Altdorf</span>
  <button id='settingsBtn'>⚙️ Settings</button>
</div>

<h1> </h1><h1> </h1><h1> </h1>

<div id='mainContent'>
<div class='relay'>
<!-- RELAY BUTTONS INSERTED BY C++ -->
</div>

<div id='leftInfo'>
  <div id='networkInfo'>
    <p id='ipInfo'>IP address: </p>
    <p id='subnetInfo'>Subnet: </p>
    <p id='macInfo'>MAC: </p>
  </div>
</div>

<!-- Overlay & config popup -->
<div id='overlay'></div>
<div id='configPopup'>
   <!-- popup content… -->
</div>

<script>
// JS inserted later by C++ for dynamic states
</script>

</body>
</html>
)=====";
