function applyScaling() {
  const baseWidth = 800;
  const baseHeight = 600;

  const scaleX = window.innerWidth / baseWidth;
  const scaleY = window.innerHeight / baseHeight;
  const scale = Math.min(scaleX, scaleY);

  const container = document.getElementById("scale-container");
  container.style.transform = `scale(${scale}) translate(-50%, -50%)`;
}

window.addEventListener("resize", applyScaling);
window.addEventListener("load", applyScaling);

const fs = require("fs");
const path = require("path");


// Pfad zur Konfigurationsdatei
const configPath = path.join(__dirname, "config.json");

const config = JSON.parse(fs.readFileSync(configPath));
document.getElementById("page-count").value = config.pages.length;

// Zurück-Button führt zurück zur index.html
document.getElementById("back-button").onclick = () => {
  navigateWithFade("index.html");
};

// "Reset All Buttons"-Funktion
document.getElementById("reset-config").onclick = () => {
  showResetConfirmation(() => {
    const emptyConfig = {
      pages: Array.from({ length: config.pages.length }, () =>
        Array.from({ length: 4 }, (_, i) => ({
          label: `Button_${i + 1}`,
          type: "shortcut",
          data: {}
        }))
      )
    };

    try {
      fs.writeFileSync(configPath, JSON.stringify(emptyConfig, null, 2));
      showToast("Alle Buttons wurden zurückgesetzt!");
    } catch (err) {
      showError("Fehler beim Zurücksetzen:\n" + err.message);
    }
  });
};

document.getElementById("update-pages").onclick = () => {
  const raw = document.getElementById("page-count").value;
  const newPageCount = parseInt(raw);

  if (isNaN(newPageCount) || newPageCount < 1 || newPageCount > 20) {
    showError("Ungültige Seitenanzahl (1–20)");
    return;
  }

  // Zwischenspeichern für später
  window._pendingPageCount = newPageCount;

  // Popup anzeigen
  document.getElementById("confirm-page-change").classList.remove("hidden");
};

document.getElementById("confirm-page-yes").onclick = () => {
  const newPageCount = window._pendingPageCount;
  const config = JSON.parse(fs.readFileSync(configPath));
  const currentCount = config.pages.length;

  if (newPageCount > currentCount) {
    const pagesToAdd = Array.from({ length: newPageCount - currentCount }, () =>
      Array.from({ length: 5 }, (_, i) => ({
        label: `Button_${i + 1}`,
        type: "shortcut",
        data: {}
      }))
    );
    config.pages = config.pages.concat(pagesToAdd);
  } else {
    config.pages = config.pages.slice(0, newPageCount);
  }

  try {
    fs.writeFileSync(configPath, JSON.stringify(config, null, 2));
    showToast("Seitenanzahl gespeichert!");
  } catch (err) {
    showError("Fehler beim Speichern:\n" + err.message);
  }

  document.getElementById("confirm-page-change").classList.add("hidden");
  window._pendingPageCount = null;
};

document.getElementById("confirm-page-no").onclick = () => {
  document.getElementById("confirm-page-change").classList.add("hidden");
  window._pendingPageCount = null;
};