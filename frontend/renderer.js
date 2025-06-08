function applyScaling() {
  const baseWidth = 800;
  const baseHeight = 600;

  const scaleX = window.innerWidth / baseWidth;
  const scaleY = window.innerHeight / baseHeight;
  const scale = Math.min(scaleX, scaleY); // gleichmäßiges Verhältnis

  const container = document.getElementById("scale-container");
  container.style.transform = `scale(${scale}) translate(-50%, -50%)`;
}

window.addEventListener("resize", applyScaling);
window.addEventListener("load", applyScaling);


const fs = require("fs");
const path = require("path");
const { exec } = require('child_process');
const os = require('os');

const configPath = path.join(__dirname, "config.json");
let config;

try {
  config = JSON.parse(fs.readFileSync(configPath));
} catch (e) {
  showError("Fehler beim Laden der Konfiguration:\n" + e.message);
  config = { buttons: [] };
}

// DOM-Elemente
const grid = document.getElementById("button-grid");
const editor = document.getElementById("editor");
const crypto = require("crypto");
const editForm = document.getElementById("edit-form");
const editIndex = document.getElementById("edit-index");
const editLabel = document.getElementById("edit-label");
const actionType = document.getElementById("action-type");
const actionConfig = document.getElementById("action-config");
const pageTitle = document.getElementById("page-title");

let currentlySelectedButton = null;
let currentPageIndex = 0;

// Buttons anzeigen
function renderButtons() {
  grid.innerHTML = ""; // Alte Buttons löschen

  const page = config.pages[currentPageIndex];
  page.forEach((btn, index) => {
    const button = document.createElement("button");
    button.className = "stream-button";
    button.innerText = btn.label || `Button ${index + 1}`;

    button.onclick = () => {
      if (index < 4) {
        const isSame = currentlySelectedButton === button;

        // Alle abwählen
        if (currentlySelectedButton) {
          currentlySelectedButton.classList.remove("selected");
          currentlySelectedButton = null;
        }

        // Wenn derselbe Button erneut geklickt wurde → Editor schließen
        if (isSame) {
          editor.style.display = "none";
          return;
        }

        // Anderen Button auswählen und anzeigen
        currentlySelectedButton = button;
        button.classList.add("selected");

        editor.style.display = "block";
        editIndex.value = index;
        editLabel.value = btn.label || "";
        const freshData = config.pages[currentPageIndex][index];  // neu aus config!
        const type = freshData.type || "shortcut";
        actionType.value = type;
        renderActionConfig(type, freshData.data || {});
      }
    };

    grid.appendChild(button);
  });

  // Seitenwechselbutton (Button 5)
  const switchButton = document.createElement("button");
  switchButton.className = "stream-button";
  switchButton.innerText = `→ Seite ${((currentPageIndex + 1) % config.pages.length) + 1}`;
  switchButton.onclick = () => {
    currentPageIndex = (currentPageIndex + 1) % config.pages.length;
    editor.style.display = "none";
    if (currentlySelectedButton) {
      currentlySelectedButton.classList.remove("selected");
      currentlySelectedButton = null;
    }
    renderButtons();
    updatePageTitle();
  };

  grid.appendChild(switchButton);
}

renderButtons(); // Initial aufrufen
updatePageTitle();

// Typänderung → neues Konfigurationsfeld erzeugen
actionType.onchange = () => {
  renderActionConfig(actionType.value, {});
};

// Zahnrad-Button klickbar machen
window.addEventListener("load", () => {
  const settingsButton = document.getElementById("settings-button");
  if (settingsButton) {
    settingsButton.addEventListener("click", () => {
      // Hier öffnest du deine Einstellungsseite
      window.location.href = "settings.html";
    });
  }
});

// Typspezifische Felder rendern
function renderActionConfig(type, data) {
  actionConfig.innerHTML = "";

  if (type === "soundboard") {
    const label = document.createElement("label");
    label.innerText = "Audio-Datei:";
    const dropZone = document.createElement("div");
    dropZone.classList.add("drop-zone");
    const shownName = data.name || data.file || "Datei hierher ziehen...";
    dropZone.innerHTML = `<span>${shownName}</span>`;
    
    dropZone.ondragover = (e) => e.preventDefault();
    dropZone.ondragenter = () => dropZone.classList.add("dragover");
    dropZone.ondragleave = () => dropZone.classList.remove("dragover");
    dropZone.ondrop = async (e) => {
      e.preventDefault();
      const file = e.dataTransfer.files[0];
      if (!file) return;

      if (!file.name.match(/\.(mp3|wav)$/i)) {
        showError("Nur MP3 oder WAV erlaubt!");
        return;
      }

      const targetDir = path.join(__dirname, "..", "backend", "sounds");
      if (!fs.existsSync(targetDir)) {
        fs.mkdirSync(targetDir, { recursive: true });
      }

      const destPath = path.join(targetDir, file.name);

      // Dateiinhalt lesen über FileReader
      const reader = new FileReader();
      reader.onload = function () {
        const arrayBuffer = reader.result;
        const buffer = Buffer.from(arrayBuffer);

        // Dateiinhalt hashen
        const hash = crypto.createHash("sha256").update(buffer).digest("hex");
        const extension = path.extname(file.name);
        const hashedFilename = `sound_${hash}${extension}`;
        const destPath = path.join(targetDir, hashedFilename);

        // Nur speichern, wenn Datei nicht bereits vorhanden ist
        if (!fs.existsSync(destPath)) {
          fs.writeFileSync(destPath, buffer);
        }

        // Speichere sowohl Dateiname (für Anzeige) als auch den tatsächlichen Pfad
        dropZone.innerHTML = `<span>${file.name}</span>`;
        dropZone.dataset.file = hashedFilename;
        dropZone.dataset.name = file.name;
      };
      reader.readAsArrayBuffer(file);
    };
    // Drop-Zone anzeigen
    dropZone.dataset.file = data.file || "";
    dropZone.dataset.name = data.name || "";
    
    actionConfig.appendChild(dropZone);

    // Lautstärke-Label
    const volumeLabel = document.createElement("label");
    volumeLabel.innerText = "Lautstärke:";
    volumeLabel.classList.add("volume-label");

    // Wrapper für Slider + Zahl
    const volumeWrapper = document.createElement("div");
    volumeWrapper.id = "soundboard-volume-wrapper";

    // Lautstärke-Slider
    const volumeSlider = document.createElement("input");
    volumeSlider.type = "range";
    volumeSlider.min = "0";
    volumeSlider.max = "200";
    volumeSlider.value = data.volume || 100;
    volumeSlider.id = "soundboard-volume";

    // Wert-Anzeige
    const volumeValue = document.createElement("span");
    volumeValue.id = "soundboard-volume-value";
    volumeValue.innerText = `${volumeSlider.value}%`;

    volumeSlider.oninput = () => {
      volumeValue.innerText = `${volumeSlider.value}%`;
    };

    volumeWrapper.appendChild(volumeSlider);
    volumeWrapper.appendChild(volumeValue);

    actionConfig.appendChild(volumeLabel);
    actionConfig.appendChild(volumeWrapper);
  }

  else if (type === "shortcut") {
    const label = document.createElement("label");
    label.innerText = "Shortcut (z. B. Ctrl+Alt+K):";
    const input = document.createElement("input");
    input.type = "text";
    input.value = data.keys || "";
    input.id = "shortcut-input";
    input.style = "width: 100%; padding: 5px;";
    actionConfig.appendChild(label);
    actionConfig.appendChild(input);
  }

else if (type === "changevolume") {
  // Ziel (Master, Firefox, etc.) als Dropdown
  const targetLabel = document.createElement("label");
  targetLabel.innerText = "Ziel:";
  const targetSelect = document.createElement("select");
  targetSelect.id = "volume-target";
  targetSelect.style.width = "100%";

  // Beispieloptionen
  //const options = ["Master", "Firefox", "Spotify", "YouTube"];
  options=[];
  if (os.platform() === "win32") {
    options = ["Master", "System", "Spotify", "Chrome", "Firefox"]; // Statische Auswahl weil Windows popokaka sage ich
    options.forEach(opt => {
      const optionElement = document.createElement("option");
      optionElement.value = opt;
      optionElement.innerText = opt;
      if ((data.target || "") === opt) optionElement.selected = true;
      targetSelect.appendChild(optionElement);
    });
  } else if(os.platform() === "linux"){
    exec('pactl list sink-inputs', (err, stdout) => {
    if (err) return console.error(err);
      options = getAppNamesFromPactlOutput(stdout);
      // nutze options hier!
      options.forEach(opt => {
      const optionElement = document.createElement("option");
      optionElement.value = opt;
      optionElement.innerText = opt;
      if ((data.target || "") === opt) optionElement.selected = true;
      targetSelect.appendChild(optionElement);
    });
  });
  }



  actionConfig.appendChild(targetLabel);
  actionConfig.appendChild(targetSelect);

  const stepAndDirWrapper = document.createElement("div");

  // Schrittgröße
  const stepField = document.createElement("div");
  stepField.className = "field-wrapper";
  const stepLabel = document.createElement("label");
  stepLabel.innerText = "Schrittgröße:";
  const stepInput = document.createElement("input");
  stepInput.type = "number";
  stepInput.min = "0";
  stepInput.max = "100";
  stepInput.value = data.step || 5;
  stepInput.id = "volume-step";
  stepInput.style.width = "100%";
  stepField.appendChild(stepLabel);
  stepField.appendChild(stepInput);

  // Richtung
  const dirField = document.createElement("div");
  dirField.className = "field-wrapper";
  const dirLabel = document.createElement("label");
  dirLabel.innerText = "Richtung:";
  const dirSelect = document.createElement("select");
  dirSelect.id = "volume-dir";
  ["+", "-", "set"].forEach(val => {
    const opt = document.createElement("option");
    opt.value = val;
    opt.innerText = val;
    if ((data.direction || "") === val) opt.selected = true;
    dirSelect.appendChild(opt);
  });
  dirSelect.style.width = "100%";
  dirField.appendChild(dirLabel);
  dirField.appendChild(dirSelect);

  // zusammensetzen
  stepAndDirWrapper.appendChild(stepField);
  stepAndDirWrapper.appendChild(dirField);
  actionConfig.appendChild(stepAndDirWrapper);
}
}

function updatePageTitle() {
  pageTitle.innerText = `Seite ${currentPageIndex + 1}`;
}

// Formular speichern
editForm.onsubmit = (e) => {
  e.preventDefault();

  const index = parseInt(editIndex.value);
  const label = editLabel.value;
  const type = actionType.value;
  let data = {};

  if (type === "soundboard") {
    const dropZone = actionConfig.querySelector("div");
    const volume = document.getElementById("soundboard-volume")?.value || "100";

    data.file = dropZone?.dataset?.file || "";

    if (dropZone?.dataset?.name) {
      data.name = dropZone.dataset.name;
    } else {
      // Name aus der aktuellen Konfiguration übernehmen, falls vorhanden
      const old = config.pages[currentPageIndex][index];
      data.name = old?.data?.name || "";
    }

    data.volume = volume;
    showToast("Gespeichert!");
  } else if (type === "shortcut") {
    const input = document.getElementById("shortcut-input");
    data.keys = input.value;
    showToast("Gespeichert!");
  } else if (type === "changevolume") {
  const rawDirection = document.getElementById("volume-dir").value;
    data = {
      target: document.getElementById("volume-target").value,
      step: document.getElementById("volume-step").value,
      direction: rawDirection === "set" ? "" : rawDirection
    };
    showToast("Gespeichert!");
  }

  try {
    config = JSON.parse(fs.readFileSync(configPath));
  } catch (err) {
    showError("Fehler beim Neuladen der Konfiguration:\n" + err.message);
  }

  config.pages[currentPageIndex][index] = { label, type, data };

  try {
    fs.writeFileSync(configPath, JSON.stringify(config, null, 2));
    console.log("Gespeichert:", config.pages[currentPageIndex][index]);
  } catch (err) {
    showError("Fehler beim Speichern:\n" + err.message);
  }

  // Button-Label im UI aktualisieren
  const btnElement = grid.children[index];
  btnElement.innerText = label || `Button ${index + 1}`;

  // Konfiguration speichern
  try {
    fs.writeFileSync(configPath, JSON.stringify(config, null, 2));
    console.log("Gespeichert:", config.pages[currentPageIndex][index]);
  } catch (err) {
    showError("Fehler beim Speichern:\n" + err.message);
  }
};

function showToast(message = "Gespeichert!") {
  const toast = document.getElementById("toast");
  toast.innerText = message;
  toast.classList.add("show");

  setTimeout(() => {
    toast.classList.remove("show");
  }, 2000); // 2 Sekunden sichtbar
}

function getAppNamesFromPactlOutput(pactlOutput) {
  const lines = pactlOutput.split('\n');
  const appNamesSet = new Set();
  appNamesSet.add("Master"); // "Master" fest als Option hinzufügen

  for (const line of lines) {
    const match = line.match(/application.name\s*=\s*"([^"]+)"/);
    if (match) {
      const name = match[1];
      if (!name.includes('sink-input-by-application')) {
        appNamesSet.add(name); // Set fügt nur einzigartige Werte hinzu
      }
    }
  }

  return Array.from(appNamesSet);
}
