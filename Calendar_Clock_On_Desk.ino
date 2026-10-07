#include <WiFi.h>
#include <Wire.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <time.h>
#include <stdlib.h>

// ================= SETTINGS =================

const char* ssid = "Amir";
const char* password = "amirshwww@09305415745";

const char* navasanToken =
  "freeIFnMe5aIPY68oTy5aRgcVA2d8cLP";

#define OLED_SDA 8
#define OLED_SCL 9
#define TOUCH_PIN 4

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(128, 64, &Wire, -1);

WebServer server(80);
Preferences preferences;

const unsigned long WEATHER_INTERVAL = 600000UL;
const unsigned long FINANCE_INTERVAL = 43200000UL;
const unsigned long BTC_INTERVAL = 60000UL;

unsigned long lastWeather = 0;
unsigned long lastFinance = 0;
unsigned long lastBitcoin = 0;

bool initialFetchDone = false;
bool serverStarted = false;

int currentScreen = 0;

// ================= DATA =================

float temperature = 0;
int humidity = 0;
int weatherCode = 0;

bool weatherLoaded = false;
bool usdLoaded = false;
bool goldLoaded = false;
bool bitcoinLoaded = false;

uint64_t usdToman = 0;
uint64_t goldToman = 0;
uint64_t bitcoinToman = 0;

// ================= TASKS =================

struct TaskItem {
  String title;
  uint32_t due;
  bool done;
  bool acknowledged;
};

const int MAX_TASKS = 12;

TaskItem tasks[MAX_TASKS];
int taskCount = 0;
int activeReminder = -1;

// ================= DATE NAMES =================

const char* monthsFinglish[] = {
  "Farvardin", "Ordibehesht", "Khordad",
  "Tir", "Mordad", "Shahrivar",
  "Mehr", "Aban", "Azar",
  "Dey", "Bahman", "Esfand"
};

const char* daysFinglish[] = {
  "Yekshanbeh", "Doshanbeh", "Seshambe",
  "Chaharshanbeh", "Panjshanbeh",
  "Jomeh", "Shanbeh"
};

// ================= WEB PAGE =================
// JavaScript uses arrow functions to avoid Arduino's
// automatic C++ prototype detection inside this string.

const char WEB_PAGE[] PROGMEM = R"ROBOLOGY(
<!DOCTYPE html>
<html lang="fa" dir="rtl">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#101827">
<title>Robology | برنامه من</title>
<style>
:root{
  color-scheme:dark;
  --bg:#101827;
  --panel:#192438;
  --line:#33445e;
  --text:#f2f6ff;
  --muted:#b4c1d5;
  --accent:#68ead4;
}
*{box-sizing:border-box}
body{
  margin:0;
  background:var(--bg);
  color:var(--text);
  font:16px/1.8 Tahoma,Arial,sans-serif;
}
button,input,select{font:inherit}
button{cursor:pointer}
button:disabled{opacity:.5;cursor:default}
main{max-width:1100px;margin:auto;padding:30px 20px}
header{
  display:flex;justify-content:space-between;
  align-items:center;gap:16px;margin-bottom:28px
}
.brand{font:700 19px Arial;color:var(--accent);letter-spacing:2px}
h1{font-size:28px;margin:0}
h2{font-size:20px;margin:0 0 20px}
.today{color:var(--muted);font-size:14px;margin-top:5px}
.layout{
  display:grid;
  grid-template-columns:minmax(0,1fr) minmax(0,1fr);
  gap:24px;align-items:start
}
.card{
  background:var(--panel);border:1px solid var(--line);
  border-radius:22px;padding:24px
}
label{display:block;margin-bottom:8px;font-weight:bold}
input,select{
  width:100%;border:1px solid var(--line);
  border-radius:12px;background:var(--bg);
  color:var(--text);padding:12px;
  min-height:48px
}
input:focus,select:focus,button:focus-visible{
  outline:2px solid var(--accent);outline-offset:3px
}
.field{margin-bottom:22px}
.calendar{
  border:1px solid var(--line);border-radius:16px;
  padding:15px;background:var(--bg)
}
.calendar-head{
  display:grid;grid-template-columns:1fr 1fr;
  gap:10px;margin-bottom:18px
}
.week,.days{display:grid;grid-template-columns:repeat(7,1fr);gap:5px}
.week span{text-align:center;color:var(--muted);font-size:14px}
.days{margin-top:9px}
.day{
  border:0;background:transparent;color:var(--text);
  border-radius:10px;min-height:42px;padding:0;
  font-size:16px
}
.day:hover{background:#293b55}
.day.today-day{box-shadow:inset 0 0 0 1px var(--accent)}
.day.selected{background:var(--accent);color:#092e29;font-weight:bold}
.selection{
  margin-top:14px;padding-top:12px;
  border-top:1px solid var(--line);
  color:var(--accent);font-size:14px
}
.time-row{display:grid;grid-template-columns:1fr 1fr;gap:14px}
.time-row label{font-size:14px;color:var(--muted)}
.primary{
  width:100%;background:var(--accent);color:#092e29;
  border:0;border-radius:12px;padding:13px;font-weight:bold
}
.note{font-size:14px;color:var(--muted);margin:14px 0 0}
#message{min-height:26px;font-size:14px;color:var(--accent);margin:12px 0 0}
.list-head{display:flex;align-items:center;justify-content:space-between;gap:10px}
.count{
  padding:2px 12px;background:#263750;
  border-radius:20px;color:var(--muted);font-size:14px
}
.task{padding:19px 0;border-top:1px solid var(--line)}
.task-title{
  direction:ltr;text-align:right;
  font-size:18px;font-weight:bold;overflow-wrap:anywhere
}
.task-date{color:var(--muted);font-size:14px;margin:7px 0}
.status{font-size:14px;color:var(--accent);margin-bottom:12px}
.overdue .status{color:#ffcb83}
.done .task-title{text-decoration:line-through;color:var(--muted)}
.actions{display:flex;gap:10px;flex-wrap:wrap}
.actions button{
  border:1px solid var(--line);
  background:#263750;color:var(--text);
  border-radius:9px;padding:7px 13px;font-size:14px
}
.actions .delete{color:#ffb9c6;background:transparent}
.empty{text-align:center;padding:40px 10px;color:var(--muted)}
.empty strong{display:block;color:var(--text);margin-bottom:8px}
@media(max-width:760px){
  .layout{grid-template-columns:1fr}
  main{padding:20px 14px}
  .card{padding:20px}
  h1{font-size:23px}
  .brand{font-size:16px}
}
</style>
</head>
<body>
<main>
<header>
  <div>
    <h1>برنامه من</h1>
    <div id="todayLabel" class="today"></div>
  </div>
  <span class="brand">ROBOLOGY</span>
</header>

<div class="layout">
<section class="card">
  <h2>تسک جدید</h2>

  <form id="taskForm">
    <div class="field">
      <label for="title">عنوان به فینگلیش</label>
      <input id="title" dir="ltr" maxlength="60"
        placeholder="Tamrin robotik" pattern="[ -~]+" required>
    </div>

    <div class="field">
      <label>تاریخ یادآوری</label>
      <div class="calendar">
        <div class="calendar-head">
          <select id="month" aria-label="ماه"></select>
          <select id="year" aria-label="سال"></select>
        </div>
        <div class="week" aria-hidden="true">
          <span>ش</span><span>ی</span><span>د</span>
          <span>س</span><span>چ</span><span>پ</span><span>ج</span>
        </div>
        <div id="days" class="days"></div>
        <div id="selection" class="selection" aria-live="polite"></div>
      </div>
    </div>

    <div class="field">
      <label>ساعت یادآوری به وقت تهران</label>
      <div class="time-row">
        <div>
          <label for="hour">ساعت</label>
          <select id="hour"></select>
        </div>
        <div>
          <label for="minute">دقیقه</label>
          <select id="minute"></select>
        </div>
      </div>
    </div>

    <button class="primary" id="submitButton" type="submit">
      ثبت تسک
    </button>

    <p id="message" role="status"></p>
    <p class="note">
      یادآوری روی نمایشگر دستگاه ظاهر می‌شود.
      با لمس دستگاه، یادآوری را تأیید کن.
    </p>
  </form>
</section>

<section class="card">
  <div class="list-head">
    <h2>تسک‌های من</h2>
    <span id="count" class="count">۰ تسک</span>
  </div>
  <div id="taskList"></div>
</section>
</div>
</main>

<script>
const $ = id => document.getElementById(id);
const fa = value => Number(value).toLocaleString('fa-IR');
const months = [
  'فروردین','اردیبهشت','خرداد','تیر','مرداد','شهریور',
  'مهر','آبان','آذر','دی','بهمن','اسفند'
];

const persianParts = new Intl.DateTimeFormat(
  'en-US-u-ca-persian-nu-latn',
  {year:'numeric',month:'numeric',day:'numeric',timeZone:'Asia/Tehran'}
);

const fullDate = new Intl.DateTimeFormat(
  'fa-IR-u-ca-persian',
  {dateStyle:'full',timeZone:'Asia/Tehran'}
);

const taskDate = new Intl.DateTimeFormat(
  'fa-IR-u-ca-persian',
  {dateStyle:'medium',timeStyle:'short',timeZone:'Asia/Tehran'}
);

const getPersian = date => {
  const parts = {};
  persianParts.formatToParts(date).forEach(part => {
    if (['year','month','day'].includes(part.type)) {
      parts[part.type] = Number(part.value);
    }
  });
  return parts;
};

const now = new Date();
const today = getPersian(now);
$('todayLabel').textContent = fullDate.format(now);

// Build a lookup from actual calendar dates.
// This also handles leap years and Esfand automatically.
const dateMap = new Map();
const baseYear = now.getUTCFullYear();

for (
  let utc = Date.UTC(baseYear-1,0,1);
  utc < Date.UTC(baseYear+4,0,1);
  utc += 86400000
) {
  const p = getPersian(new Date(utc));
  dateMap.set(`${p.year}-${p.month}-${p.day}`, utc);
}

months.forEach((name,index) => {
  $('month').add(new Option(name,String(index+1)));
});

for (let year=today.year; year<=today.year+2; year++) {
  $('year').add(new Option(fa(year),String(year)));
}

for (let hour=0; hour<24; hour++) {
  $('hour').add(new Option(
    String(hour).padStart(2,'0'),String(hour)
  ));
}

for (let minute=0; minute<60; minute++) {
  $('minute').add(new Option(
    String(minute).padStart(2,'0'),String(minute)
  ));
}

$('month').value = String(today.month);
$('year').value = String(today.year);

const clockParts = {};
new Intl.DateTimeFormat('en-GB',{
  timeZone:'Asia/Tehran',
  hour:'2-digit',
  minute:'2-digit',
  hourCycle:'h23'
}).formatToParts(new Date(Date.now()+5*60000)).forEach(part => {
  clockParts[part.type] = part.value;
});

$('hour').value = String(Number(clockParts.hour));
$('minute').value = String(Number(clockParts.minute));

let selected = {
  year:today.year,
  month:today.month,
  day:today.day
};

const drawCalendar = () => {
  const year = Number($('year').value);
  const month = Number($('month').value);
  const container = $('days');
  container.replaceChildren();

  const first = dateMap.get(`${year}-${month}-1`);
  if (first === undefined) return;

  const firstWeekday = (new Date(first).getUTCDay()+1)%7;

  for (let i=0; i<firstWeekday; i++) {
    const spacer = document.createElement('span');
    container.append(spacer);
  }

  for (let day=1; day<=31; day++) {
    if (!dateMap.has(`${year}-${month}-${day}`)) break;

    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'day';
    button.textContent = fa(day);
    button.setAttribute('aria-label',
      `${fa(day)} ${months[month-1]} ${fa(year)}`
    );

    const isSelected =
      selected &&
      selected.year===year &&
      selected.month===month &&
      selected.day===day;

    button.setAttribute('aria-pressed',String(Boolean(isSelected)));

    if (isSelected) button.classList.add('selected');

    if (
      year===today.year &&
      month===today.month &&
      day===today.day
    ) {
      button.classList.add('today-day');
    }

    button.onclick = () => {
      selected = {year,month,day};
      drawCalendar();
    };

    container.append(button);
  }

  $('selection').textContent = selected
    ? `انتخاب شما: ${fa(selected.day)} ${months[selected.month-1]} ${fa(selected.year)}`
    : 'یک روز را از تقویم انتخاب کن.';
};

const changeMonth = () => {
  selected = null;
  drawCalendar();
};

$('month').onchange = changeMonth;
$('year').onchange = changeMonth;
drawCalendar();

const api = async (path,body) => {
  const response = await fetch(
    path,
    body ? {method:'POST',body:new URLSearchParams(body)} : {}
  );

  const data = await response.json();

  if (!response.ok) {
    throw new Error(data.error || 'ارتباط با دستگاه ناموفق بود.');
  }

  return data;
};

const changeTask = async (path,id) => {
  try {
    await api(path,{id:String(id)});
    await refresh();
  } catch(error) {
    $('message').textContent = error.message;
  }
};

const refresh = async () => {
  try {
    const data = await api('/api/tasks');
    const list = $('taskList');
    list.replaceChildren();

    const pending = data.tasks.filter(task => !task.done).length;
    $('count').textContent = `${fa(pending)} تسک باقی‌مانده`;

    if (!data.tasks.length) {
      const empty = document.createElement('div');
      empty.className = 'empty';

      const heading = document.createElement('strong');
      heading.textContent = 'برای برنامه‌ات جا باز کن';

      const detail = document.createElement('div');
      detail.textContent = 'اولین تسک را با یک تاریخ و ساعت ثبت کن.';

      empty.append(heading,detail);
      list.append(empty);
      return;
    }

    const sorted = [...data.tasks].sort(
      (a,b) => Number(a.done)-Number(b.done) || a.due-b.due
    );

    sorted.forEach(task => {
      const overdue = !task.done && task.due<=Date.now()/1000;
      const row = document.createElement('div');

      row.className = 'task' +
        (task.done ? ' done' : '') +
        (overdue ? ' overdue' : '');

      const title = document.createElement('div');
      title.className = 'task-title';
      title.textContent = task.title;

      const date = document.createElement('div');
      date.className = 'task-date';
      date.textContent = taskDate.format(new Date(task.due*1000));

      const status = document.createElement('div');
      status.className = 'status';
      status.textContent = task.done
        ? 'انجام‌شده'
        : task.acknowledged
        ? 'یادآوری تأیید شده'
        : overdue
        ? 'زمان این تسک رسیده'
        : 'در انتظار یادآوری';

      const actions = document.createElement('div');
      actions.className = 'actions';

      if (!task.done) {
        const done = document.createElement('button');
        done.type = 'button';
        done.textContent = 'انجام شد';
        done.onclick = () => changeTask('/api/done',task.id);
        actions.append(done);
      }

      const remove = document.createElement('button');
      remove.type = 'button';
      remove.className = 'delete';
      remove.textContent = 'حذف';

      remove.onclick = () => {
        if (confirm('این تسک حذف شود؟')) {
          changeTask('/api/delete',task.id);
        }
      };

      actions.append(remove);
      row.append(title,date,status,actions);
      list.append(row);
    });
  } catch(error) {
    $('message').textContent = error.message;
  }
};

$('taskForm').onsubmit = async event => {
  event.preventDefault();

  const title = $('title').value.trim();

  if (!/^[ -~]{1,60}$/.test(title)) {
    $('message').textContent = 'عنوان را به فینگلیش وارد کن.';
    return;
  }

  if (!selected) {
    $('message').textContent = 'یک روز را از تقویم انتخاب کن.';
    return;
  }

  const key = `${selected.year}-${selected.month}-${selected.day}`;
  const utc = dateMap.get(key);

  if (utc === undefined) {
    $('message').textContent = 'تاریخ انتخاب‌شده معتبر نیست.';
    return;
  }

  // Tehran time = UTC + 03:30.
  const due = Math.floor(utc/1000)
    + Number($('hour').value)*3600
    + Number($('minute').value)*60
    - 12600;

  if (due<=Date.now()/1000) {
    $('message').textContent = 'تاریخ و ساعت یادآوری باید در آینده باشد.';
    return;
  }

  $('submitButton').disabled = true;

  try {
    await api('/api/tasks',{title,due:String(due)});
    $('title').value = '';
    $('message').textContent = 'تسک ذخیره شد؛ دستگاه در زمان انتخاب‌شده یادآوری می‌کند.';
    await refresh();
  } catch(error) {
    $('message').textContent = error.message;
  } finally {
    $('submitButton').disabled = false;
  }
};

refresh();
setInterval(refresh,10000);
</script>
</body>
</html>
)ROBOLOGY";

// ================= DISPLAY HELPERS =================

void centered(const String& text, int y, int size) {
  display.setTextSize(size);

  int16_t x1, y1;
  uint16_t width, height;

  display.getTextBounds(
    text, 0, y,
    &x1, &y1, &width, &height
  );

  display.setCursor((128 - (int)width) / 2, y);
  display.print(text);
}

void drawIcon(int page) {
  if (page == 0) {
    display.drawCircle(8, 7, 5, SSD1306_WHITE);
    display.drawLine(8, 7, 8, 3, SSD1306_WHITE);
    display.drawLine(8, 7, 11, 8, SSD1306_WHITE);
  } else if (page == 1) {
    display.drawRoundRect(2, 2, 12, 11, 2, SSD1306_WHITE);
    display.drawLine(3, 5, 13, 5, SSD1306_WHITE);
    display.drawPixel(6, 8, SSD1306_WHITE);
    display.drawPixel(10, 8, SSD1306_WHITE);
  } else if (page == 2) {
    display.drawCircle(6, 5, 3, SSD1306_WHITE);
    display.fillRoundRect(4, 8, 11, 5, 2, SSD1306_WHITE);
  } else {
    display.drawCircle(8, 7, 6, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(6, 4);

    const char* symbols[] = {"", "", "", "$", "G", "B", "T"};
    display.print(symbols[page]);
  }
}

void drawFrame(const char* title) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  drawIcon(currentScreen);

  display.setTextSize(1);
  display.setCursor(20, 4);
  display.print(title);

  if (WiFi.status() == WL_CONNECTED) {
    display.fillCircle(123, 7, 2, SSD1306_WHITE);
  } else {
    display.drawCircle(123, 7, 2, SSD1306_WHITE);
  }

  display.drawFastHLine(0, 16, 128, SSD1306_WHITE);
  display.drawFastHLine(0, 54, 128, SSD1306_WHITE);

  for (int i = 0; i < 7; i++) {
    int x = 39 + i * 8;

    if (i == currentScreen) {
      display.fillRoundRect(x, 58, 6, 4, 1, SSD1306_WHITE);
    } else {
      display.drawPixel(x + 2, 59, SSD1306_WHITE);
    }
  }

  display.setTextSize(1);
  display.setCursor(2, 57);
  display.print(currentScreen + 1);
  display.print("/7");
}

String formatMoney(uint64_t value) {
  char buffer[24];

  snprintf(
    buffer, sizeof(buffer),
    "%llu", (unsigned long long)value
  );

  String raw(buffer);
  String result;

  for (unsigned int i = 0; i < raw.length(); i++) {
    if (i > 0 && (raw.length() - i) % 3 == 0) {
      result += ',';
    }

    result += raw[i];
  }

  return result;
}

void drawPrice(uint64_t price, bool loaded, const char* unit) {
  if (!loaded) {
    centered("No data yet", 25, 1);
    centered("Check connection", 41, 1);
    return;
  }

  String text = formatMoney(price);
  int size = text.length() <= 10 ? 2 : 1;

  centered(text, size == 2 ? 25 : 29, size);
  centered(unit, 44, 1);
}

// ================= CALENDAR =================

void gregorianToJalali(
  int gy, int gm, int gd,
  int& jy, int& jm, int& jd
) {
  const int gDays[] = {
    31,28,31,30,31,30,31,31,30,31,30,31
  };

  const int jDays[] = {
    31,31,31,31,31,31,30,30,30,30,30,29
  };

  int gy2 = gy - 1600;
  int gm2 = gm - 1;

  long gDayNo =
    365L * gy2 +
    (gy2 + 3) / 4 -
    (gy2 + 99) / 100 +
    (gy2 + 399) / 400;

  for (int i = 0; i < gm2; i++) {
    gDayNo += gDays[i];
  }

  if (
    gm2 > 1 &&
    ((gy % 4 == 0 && gy % 100 != 0) || gy % 400 == 0)
  ) {
    gDayNo++;
  }

  gDayNo += gd - 1;

  long jDayNo = gDayNo - 79;
  long jNp = jDayNo / 12053;

  jDayNo %= 12053;

  jy = 979 + 33 * jNp + 4 * (jDayNo / 1461);

  jDayNo %= 1461;

  if (jDayNo >= 366) {
    jy += (jDayNo - 1) / 365;
    jDayNo = (jDayNo - 1) % 365;
  }

  int i = 0;

  while (i < 11 && jDayNo >= jDays[i]) {
    jDayNo -= jDays[i];
    i++;
  }

  jm = i + 1;
  jd = jDayNo + 1;
}

// ================= WEATHER =================

const char* weatherText(int code) {
  if (code == 0) return "Clear";
  if (code >= 1 && code <= 3) return "Cloudy";
  if (code == 45 || code == 48) return "Foggy";
  if (code >= 51 && code <= 67) return "Rainy";
  if (code >= 71 && code <= 77) return "Snowy";
  if (code >= 80 && code <= 82) return "Showers";
  if (code == 85 || code == 86) return "Snow showers";
  if (code >= 95) return "Thunderstorm";
  return "Unknown";
}

void updateWeather() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;

  http.setConnectTimeout(3000);
  http.setTimeout(3500);

  const char* url =
    "http://api.open-meteo.com/v1/forecast?"
    "latitude=35.6944&longitude=51.4215&"
    "current=temperature_2m,relative_humidity_2m,weather_code";

  if (!http.begin(client, url)) return;

  int code = http.GET();

  if (code == HTTP_CODE_OK) {
    JsonDocument doc;

    if (!deserializeJson(doc, http.getString())) {
      JsonObject current = doc["current"].as<JsonObject>();

      if (
        !current["temperature_2m"].isNull() &&
        !current["relative_humidity_2m"].isNull() &&
        !current["weather_code"].isNull()
      ) {
        temperature = current["temperature_2m"].as<float>();
        humidity = current["relative_humidity_2m"].as<int>();
        weatherCode = current["weather_code"].as<int>();
        weatherLoaded = true;
      }
    }
  } else {
    Serial.printf("Weather HTTP: %d\n", code);
  }

  http.end();
}

// ================= USD AND GOLD =================

void updateFinance() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(5);

  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(3500);

  String url =
    "https://api.navasan.tech/latest/?api_key=" +
    String(navasanToken);

  if (!http.begin(client, url)) return;

  int code = http.GET();

  if (code == HTTP_CODE_OK) {
    JsonDocument doc;

    if (!deserializeJson(doc, http.getString())) {
      uint64_t usd =
        doc["usd_sell"]["value"].as<uint64_t>();

      if (usd > 0) {
        // Navasan value is used directly as toman.
        usdToman = usd;
        usdLoaded = true;
      }

      uint64_t gold =
        doc["18ayar"]["value"].as<uint64_t>();

      if (gold == 0) {
        gold = doc["geram18"]["value"].as<uint64_t>();
      }

      if (gold > 0) {
        goldToman = gold;
        goldLoaded = true;
      }
    }
  } else {
    Serial.printf("Navasan HTTP: %d\n", code);
  }

  http.end();
}

// ================= BITCOIN =================

void updateBitcoin() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(5);

  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(3500);

  const char* url =
    "https://apiv2.nobitex.ir/market/stats?"
    "srcCurrency=btc&dstCurrency=rls";

  if (!http.begin(client, url)) return;

  int code = http.GET();

  if (code == HTTP_CODE_OK) {
    JsonDocument doc;

    if (
      !deserializeJson(doc, http.getString()) &&
      doc["status"].as<String>() == "ok"
    ) {
      uint64_t rial =
        doc["stats"]["btc-rls"]["latest"].as<uint64_t>();

      if (rial > 0) {
        bitcoinToman = rial / 10ULL;
        bitcoinLoaded = true;
      }
    }
  } else {
    Serial.printf("Nobitex HTTP: %d\n", code);
  }

  http.end();
}

// ================= SAVE / LOAD TASKS =================

void saveTasks() {
  JsonDocument doc;
  JsonArray array = doc["tasks"].to<JsonArray>();

  for (int i = 0; i < taskCount; i++) {
    JsonObject item = array.add<JsonObject>();

    item["title"] = tasks[i].title;
    item["due"] = tasks[i].due;
    item["done"] = tasks[i].done;
    item["ack"] = tasks[i].acknowledged;
  }

  String data;
  serializeJson(doc, data);

  preferences.putString("tasks", data);
}

void loadTasks() {
  preferences.begin("robology", false);

  String data = preferences.getString("tasks", "{}");

  JsonDocument doc;

  if (deserializeJson(doc, data)) return;

  for (JsonObject item : doc["tasks"].as<JsonArray>()) {
    if (taskCount >= MAX_TASKS) break;

    tasks[taskCount].title = item["title"].as<String>();
    tasks[taskCount].due = item["due"].as<uint32_t>();
    tasks[taskCount].done = item["done"].as<bool>();
    tasks[taskCount].acknowledged = item["ack"].as<bool>();

    taskCount++;
  }
}

// ================= WEB API =================

void sendError(int status, const char* message) {
  JsonDocument doc;
  doc["error"] = message;

  String data;
  serializeJson(doc, data);

  server.send(
    status,
    "application/json; charset=utf-8",
    data
  );
}

int requestedTaskId() {
  String raw = server.arg("id");

  if (raw.length() == 0) return -1;

  char* end;
  long id = strtol(raw.c_str(), &end, 10);

  if (*end != '\0' || id < 0 || id >= taskCount) {
    return -1;
  }

  return (int)id;
}

void handleTaskList() {
  JsonDocument doc;
  JsonArray array = doc["tasks"].to<JsonArray>();

  for (int i = 0; i < taskCount; i++) {
    JsonObject item = array.add<JsonObject>();

    item["id"] = i;
    item["title"] = tasks[i].title;
    item["due"] = tasks[i].due;
    item["done"] = tasks[i].done;
    item["acknowledged"] = tasks[i].acknowledged;
  }

  String data;
  serializeJson(doc, data);

  server.send(200, "application/json; charset=utf-8", data);
}

void handleAddTask() {
  if (taskCount >= MAX_TASKS) {
    sendError(400, "حداکثر ۱۲ تسک؛ تسک‌های قبلی را حذف کن.");
    return;
  }

  String title = server.arg("title");
  title.trim();

  if (title.length() == 0 || title.length() > 60) {
    sendError(400, "عنوان باید بین ۱ تا ۶۰ نویسه باشد.");
    return;
  }

  for (unsigned int i = 0; i < title.length(); i++) {
    unsigned char c = (unsigned char)title[i];

    if (c < 32 || c > 126) {
      sendError(400, "عنوان را به فینگلیش وارد کن.");
      return;
    }
  }

  String rawDue = server.arg("due");

  if (rawDue.length() == 0) {
    sendError(400, "زمان یادآوری را وارد کن.");
    return;
  }

  char* end;
  unsigned long due = strtoul(rawDue.c_str(), &end, 10);
  time_t now = time(nullptr);

  if (
    *end != '\0' ||
    due < 1700000000UL ||
    (now > 1700000000 && due <= (uint32_t)now)
  ) {
    sendError(400, "زمان یادآوری معتبر نیست یا گذشته است.");
    return;
  }

  tasks[taskCount].title = title;
  tasks[taskCount].due = (uint32_t)due;
  tasks[taskCount].done = false;
  tasks[taskCount].acknowledged = false;

  taskCount++;
  saveTasks();

  server.send(200, "application/json", "{\"ok\":true}");
}

void handleDoneTask() {
  int id = requestedTaskId();

  if (id < 0) {
    sendError(400, "تسک یافت نشد.");
    return;
  }

  tasks[id].done = true;
  tasks[id].acknowledged = true;

  if (activeReminder == id) {
    activeReminder = -1;
  }

  saveTasks();

  server.send(200, "application/json", "{\"ok\":true}");
}

void handleDeleteTask() {
  int id = requestedTaskId();

  if (id < 0) {
    sendError(400, "تسک یافت نشد.");
    return;
  }

  if (activeReminder == id) {
    activeReminder = -1;
  } else if (activeReminder > id) {
    activeReminder--;
  }

  for (int i = id; i < taskCount - 1; i++) {
    tasks[i] = tasks[i + 1];
  }

  taskCount--;
  saveTasks();

  server.send(200, "application/json", "{\"ok\":true}");
}

void startWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send_P(
      200,
      "text/html; charset=utf-8",
      WEB_PAGE
    );
  });

  server.on("/api/tasks", HTTP_GET, handleTaskList);
  server.on("/api/tasks", HTTP_POST, handleAddTask);
  server.on("/api/done", HTTP_POST, handleDoneTask);
  server.on("/api/delete", HTTP_POST, handleDeleteTask);

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
  serverStarted = true;

  Serial.print("Tasks web page: http://");
  Serial.println(WiFi.localIP());
}

// ================= REMINDERS =================

void checkReminder() {
  time_t now = time(nullptr);

  if (now < 1700000000 || activeReminder >= 0) return;

  int selected = -1;

  for (int i = 0; i < taskCount; i++) {
    if (
      !tasks[i].done &&
      !tasks[i].acknowledged &&
      tasks[i].due <= (uint32_t)now
    ) {
      if (selected < 0 || tasks[i].due < tasks[selected].due) {
        selected = i;
      }
    }
  }

  if (selected >= 0) {
    activeReminder = selected;
    currentScreen = 6;
  }
}

void drawTaskTitle(const String& title) {
  for (int line = 0; line < 3; line++) {
    int start = line * 20;

    if (start >= (int)title.length()) break;

    String part = title.substring(start, start + 20);
    centered(part, 20 + line * 9, 1);
  }
}

void drawTasks() {
  if (activeReminder >= 0) {
    drawTaskTitle(tasks[activeReminder].title);
    centered("Touch to dismiss", 46, 1);

    if ((millis() / 500) % 2 == 0) {
      display.drawRect(0, 17, 128, 37, SSD1306_WHITE);
    }

    return;
  }

  int selected = -1;

  for (int i = 0; i < taskCount; i++) {
    if (!tasks[i].done) {
      if (selected < 0 || tasks[i].due < tasks[selected].due) {
        selected = i;
      }
    }
  }

  if (selected < 0) {
    centered("No pending tasks", 23, 1);

    if (WiFi.status() == WL_CONNECTED) {
      centered(WiFi.localIP().toString(), 41, 1);
    } else {
      centered("WiFi disconnected", 41, 1);
    }

    return;
  }

  drawTaskTitle(tasks[selected].title);

  time_t due = tasks[selected].due;
  struct tm info;
  localtime_r(&due, &info);

  char buffer[24];
  strftime(buffer, sizeof(buffer), "%m/%d %H:%M", &info);

  centered(buffer, 46, 1);
}

// ================= TOUCH =================

void updateTouch() {
  static bool previousRaw = LOW;
  static bool stableState = LOW;
  static unsigned long changedAt = 0;

  bool raw = digitalRead(TOUCH_PIN);

  if (raw != previousRaw) {
    previousRaw = raw;
    changedAt = millis();
  }

  if (millis() - changedAt >= 40 && raw != stableState) {
    stableState = raw;

    if (stableState == HIGH) {
      if (activeReminder >= 0) {
        tasks[activeReminder].acknowledged = true;
        activeReminder = -1;
        saveTasks();
      } else {
        currentScreen = (currentScreen + 1) % 7;
      }
    }
  }
}

// ================= DRAW SCREEN =================

void drawScreen() {
  struct tm info;
  bool hasTime = getLocalTime(&info, 0);

  const char* titles[] = {
    "CLOCK",
    "JALALI DATE",
    "TEHRAN WEATHER",
    "USD RATE",
    "GOLD 18K",
    "BITCOIN",
    "MY TASKS"
  };

  drawFrame(
    activeReminder >= 0 ? "REMINDER" : titles[currentScreen]
  );

  if (currentScreen == 0) {
    if (hasTime) {
      char buffer[16];

      snprintf(
        buffer, sizeof(buffer),
        "%02d:%02d",
        info.tm_hour, info.tm_min
      );

      centered(buffer, 21, 3);

      snprintf(
        buffer, sizeof(buffer),
        "SEC %02d", info.tm_sec
      );

      centered(buffer, 46, 1);
    } else {
      centered("Syncing clock...", 26, 1);
      centered("Connect to WiFi", 43, 1);
    }
  } else if (currentScreen == 1) {
    if (hasTime) {
      int jy, jm, jd;

      gregorianToJalali(
        info.tm_year + 1900,
        info.tm_mon + 1,
        info.tm_mday,
        jy, jm, jd
      );

      display.setTextSize(3);
      display.setCursor(5, 23);
      display.print(jd);

      display.drawFastVLine(45, 21, 29, SSD1306_WHITE);

      display.setTextSize(1);

      display.setCursor(49, 21);
      display.print(daysFinglish[info.tm_wday]);

      display.setCursor(49, 33);
      display.print(monthsFinglish[jm - 1]);

      display.setCursor(49, 45);
      display.print(jy);
    } else {
      centered("Syncing date...", 30, 1);
    }
  } else if (currentScreen == 2) {
    if (weatherLoaded) {
      char buffer[20];

      snprintf(
        buffer, sizeof(buffer),
        "%.1f C", temperature
      );

      centered(buffer, 21, 2);
      centered(weatherText(weatherCode), 38, 1);
      centered("Humidity " + String(humidity) + "%", 46, 1);
    } else {
      centered("No weather data", 30, 1);
    }
  } else if (currentScreen == 3) {
    drawPrice(usdToman, usdLoaded, "TOMAN / USD");
  } else if (currentScreen == 4) {
    drawPrice(goldToman, goldLoaded, "TOMAN / GRAM");
  } else if (currentScreen == 5) {
    drawPrice(bitcoinToman, bitcoinLoaded, "TOMAN / BTC");
  } else {
    drawTasks();
  }

  display.display();
}

// ================= SETUP =================

void setup() {
  Serial.begin(115200);

  pinMode(TOUCH_PIN, INPUT);

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found!");

    while (true) {
      delay(100);
    }
  }

  display.setTextWrap(false);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();

  centered("ROBOLOGY", 16, 2);
  centered("SMART DESK CLOCK", 40, 1);

  display.display();

  loadTasks();

  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  configTime(
    3 * 3600 + 30 * 60,
    0,
    "pool.ntp.org",
    "time.google.com"
  );
}

// ================= LOOP =================

void loop() {
  static unsigned long lastReconnect = 0;
  static unsigned long lastFrame = 0;

  updateTouch();

  if (WiFi.status() == WL_CONNECTED && !serverStarted) {
    startWebServer();
  }

  if (serverStarted) {
    server.handleClient();
  }

  checkReminder();

  if (millis() - lastFrame >= 100) {
    lastFrame = millis();
    drawScreen();
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastReconnect >= 15000UL) {
      lastReconnect = millis();
      WiFi.reconnect();
    }

    return;
  }

  // Keep reminder visible and responsive.
  if (activeReminder >= 0) return;

  // HTTP requests can briefly pause display/touch updates.
  if (!initialFetchDone) {
    updateWeather();
    server.handleClient();

    updateFinance();
    server.handleClient();

    updateBitcoin();

    lastWeather = millis();
    lastFinance = millis();
    lastBitcoin = millis();

    initialFetchDone = true;
    return;
  }

  if (millis() - lastWeather >= WEATHER_INTERVAL) {
    lastWeather = millis();
    updateWeather();
    return;
  }

  if (millis() - lastFinance >= FINANCE_INTERVAL) {
    lastFinance = millis();
    updateFinance();
    return;
  }

  if (millis() - lastBitcoin >= BTC_INTERVAL) {
    lastBitcoin = millis();
    updateBitcoin();
  }
}