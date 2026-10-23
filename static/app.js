const result = document.querySelector("#result");
const status = document.querySelector("#status");
const authPanel = document.querySelector("#auth-panel");
const emailInput = document.querySelector('#register-form [name="email"]');
const meButton = document.querySelector("#me-button");

// Keep the access token in page memory; a reload clears it.
let accessToken = null;

async function requestJson(path, options = {}) {
  const response = await fetch(path, options);
  const contentType = response.headers.get("content-type") || "";
  const body = contentType.includes("application/json")
    ? await response.json()
    : await response.text();
  if (!response.ok) {
    const message = typeof body === "object" ? body.message : body;
    throw new Error(message || `Request failed (${response.status})`);
  }
  return body;
}

function show(value) {
  result.textContent = typeof value === "string" ? value : JSON.stringify(value, null, 2);
}

document.querySelector("#ping-button").addEventListener("click", async () => {
  status.textContent = "Contacting API…";
  try {
    const response = await requestJson("/api/ping");
    status.textContent = "Server is available";
    show(response);
  } catch (error) {
    status.textContent = "Request failed";
    show(error.message);
  }
});

document.querySelector("#register-form").addEventListener("submit", async event => {
  event.preventDefault();
  const form = new FormData(event.currentTarget);
  const payload = {
    username: form.get("username"),
    password: form.get("password")
  };
  if (form.get("email")) payload.email = form.get("email");

  try {
    show(await requestJson("/api/auth/register", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    }));
  } catch (error) {
    show(error.message);
  }
});

document.querySelector("#login-form").addEventListener("submit", async event => {
  event.preventDefault();
  const form = new FormData(event.currentTarget);
  try {
    const response = await requestJson("/api/auth/login", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        username: form.get("username"),
        password: form.get("password")
      })
    });
    accessToken = response.access_token;
    meButton.disabled = false;
    show({ message: "Signed in", token_type: response.token_type, expires_in: response.expires_in });
  } catch (error) {
    accessToken = null;
    meButton.disabled = true;
    show(error.message);
  }
});

meButton.addEventListener("click", async () => {
  if (!accessToken) return;
  try {
    show(await requestJson("/api/auth/me", {
      headers: { Authorization: `Bearer ${accessToken}` }
    }));
  } catch (error) {
    show(error.message);
  }
});

(async () => {
  try {
    const info = await requestJson("/api/info");
    const endpoints = info.endpoints || [];
    const authEnabled = endpoints.some(endpoint => endpoint.path === "/api/auth/login");
    authPanel.hidden = !authEnabled;
    const emailRequired = Boolean(info.registration_email_required);
    emailInput.required = emailRequired;
    emailInput.placeholder = emailRequired ? "Required" : "Optional";
    status.textContent = "Server is available";
    show(info);
  } catch (error) {
    status.textContent = "API unavailable";
    show(error.message);
  }
})();
