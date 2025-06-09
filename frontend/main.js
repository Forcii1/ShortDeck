const { app, BrowserWindow } = require('electron');
const path = require("path");

function createWindow() {
  const platform = process.platform;
  let iconPath;

  if (platform === "win32") {
    iconPath = path.join(__dirname, "assets", "AppIcon.ico");
  } else {
    iconPath = path.join(__dirname, "assets", "AppIcon.png");
  }

  const win = new BrowserWindow({
    width: 870,
    height: 700,
    minWidth: 435,
    minHeight: 350,
    backgroundColor: '#1e1e1e',
    show: false,
    icon: iconPath,
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false
    }
  });

  win.loadFile('splash.html');

  win.once('ready-to-show', () => {
    win.show();
  });
}

app.whenReady().then(createWindow);