function doGet(e) {
 params = JSON.stringify(myFunction());
  return ContentService.createTextOutput(params).setMimeType(ContentService.MimeType.JSON);
}
function myFunction(n) {
  days = ["Dim","Lun","Mar","Mer","Jeu","Ven","Sam"]
  months = ["JAN","FÉV","MAR","AVR","MAI","JUIN","JUIL","AOÛ","SEP","OCT","NOV","DÉC"]
  var today = new Date();
  offset = today.getTimezoneOffset()
  year = today.getFullYear()
  month = months[today.getMonth()]
  day = days[today.getDay()]
  date = today.getDate()
  hour = today.getHours()
  min = today.getMinutes()
  

  Logger.log(offset)
  Logger.log(year)
  Logger.log(month)
  Logger.log(day)
  Logger.log(date)
  Logger.log(hour)
  Logger.log(min)
  Logger.log(today);
  var cal = CalendarApp.getCalendarById("monk.binder@gmail.com");
  Logger.log(cal.getName())
  Logger.log(cal.getId())
  var events = cal.getEventsForDay(today);
  Logger.log(events.length.toString());
  var list = { "offset": offset, "year": year, "month": month, "day": day, "date": date, "hour": hour, "minute": min };
  for (i=0; i < events.length; i++) {
    Logger.log("Here: " + i.toString());
              list[i] = getStartLocale(events[i]);
  }
  Logger.log(list);
  return (list)
}

function getStartLocale(event) {
  localetz = CalendarApp.getCalendarById("monk.binder@gmail.com").getTimeZone()
  return (event.getStartTime().toLocaleString("en-US", {timezone: localetz}))
}