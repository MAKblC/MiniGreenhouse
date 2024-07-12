function doPost(e) {
  var ss = SpreadsheetApp.openById("1mXXXXXXXXXXXXXXXXXXXwvI"); // URL таблицы
  var sheet = ss.getSheetByName("greenhouse_mini"); // имя листа
  var date = String(e.parameter.date);
  var light = Number(e.parameter.light);
  var temp = Number(e.parameter.temp);
  var hum = Number(e.parameter.hum);
  var press = Number(e.parameter.press);
  var tempH = Number(e.parameter.tempH);
  var humH = Number(e.parameter.humH);
  sheet.appendRow([date, light, temp, hum, press, tempH, humH]);
}
