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

// Zurück-Button führt zurück zur index.html
document.getElementById("back-button").onclick = () => {
  window.location.href = "index.html";
};

// "Reset All Buttons"-Funktion
document.getElementById("reset-config").onclick = () => {
  showResetConfirmation(() => {
    const emptyConfig = {
      pages: Array.from({ length: 3 }, () =>
        Array.from({ length: 4 }, (_, i) => ({
          label: `Button_${i + 1}`,
          type: "shortcut",
          data: {}
        }))
      )
    };

    try {
      fs.writeFileSync(configPath, JSON.stringify(emptyConfig, null, 2));
      showError("Alle Buttons wurden zurückgesetzt!");
    } catch (err) {
      showError("Fehler beim Zurücksetzen:\n" + err.message);
    }
  });
};

// Popup-Funktion anzeigen
function showResetConfirmation(onConfirm) {
  const dialog = document.getElementById("confirm-reset");
  dialog.classList.remove("hidden");

  document.getElementById("confirm-yes").onclick = () => {
    dialog.classList.add("hidden");
    onConfirm(); // führt Reset aus
  };

  document.getElementById("confirm-no").onclick = () => {
    dialog.classList.add("hidden");
  };
}

// Error-Popop
function showError(message) {
  const popup = document.getElementById("error-popup");
  document.getElementById("error-message").textContent = message;
  popup.classList.remove("hidden");
}

document.getElementById("error-ok").onclick = () => {
  document.getElementById("error-popup").classList.add("hidden");
};