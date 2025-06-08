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