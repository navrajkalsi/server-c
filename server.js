const files_list = document.querySelector("ul"), // Gets the first ul element
  file_preview = document.getElementById("file-preview"),
  response_image = document.getElementById("response-image");

// This is the 'li' element which would be selected
let selected_file = null,
  abort_controller = null, // Global abort_controller, needs to be defined for every request
  response_content_type = null, // Content-Type header of the current response, used to decide whether the re-render the whole 'file-preview' eleemnt with .innerHTML or just use .textContent
  current_url = window.location.href,
  current_path = new URL(current_url).pathname;

// Just so further requests do not contain double /
// Won't cause any errors if it does but looks weird
// As long as the url itself is not '/'
if (current_path != "/") {
  const new_url = simplify_url(current_url);
  if (new_url != current_url)
    window.location.href = new_url;
}
else
  current_path = "./";

// Simplifies a url by removing '/' from the end and replacing '//' with '/' other than in 'http://localhost'
function simplify_url(url) {
  if (!url)
    return;

  // Replacing '//' with '/'
  let protocol_slash_index = 0;

  if (url.startsWith("http://") || url.startsWith("https://"))
    protocol_slash_index = url.indexOf("//");

  // Getting index of first '//' if protocol is included and then searching from there to end

  while (true) {
    let index = url.indexOf("//", protocol_slash_index + 1);
    // -1 if the substring does not exist
    if (index == -1)
      break;
    url = url.slice(0, index) + url.slice(index + 1);
  }

  // No need to remove last '/' if there is only one '/' at the end after the domain name
  if (url.indexOf("/", protocol_slash_index + 2) != url.length - 1 || url.startsWith("/"))
    if (url.endsWith("/"))
      url = url.slice(0, -1);

  return url;
}

// Takes in a relative url/path and make a request for the current_url + url
function visit_path(path) {
  if (!path)
    return;

  window.location.href = simplify_url(current_url + "/" + path);
}

// Displays the preview of a directory by selecting just the <ul> of the html response
// Accepts response.text() promise
function dir_preview(response_text) {
  // Using innerHTML instead of textContent cause I want to render the ul element in the DOM and not just copy and paste the response in pre tag
  file_preview.innerHTML = response_text.substring(response_text.indexOf("<ul"), response_text.indexOf("</ul>") + 5);
}

// Displays preview of an image by creating an <img>
// Accepts response.blob() promise
function img_preview(response_blob) {
  let preview = file_preview.querySelector("img#img-preview");

  if (!preview) {
    preview = document.createElement("img");
    preview.id = "img-preview";

    // Clearing any previous response of any other tag/response type
    if (file_preview.firstElementChild)
      file_preview.innerHTML = "";
    file_preview.appendChild(preview);
  }

  preview.src = URL.createObjectURL(response_blob);
}

// Displays preivew of a text file by creating a <pre> to preserve formatting
// Accepts response.text() promise
function text_preview(response_text) {
  let preview = file_preview.querySelector("pre#text-preview");

  if (!preview) {
    preview = document.createElement("pre");
    preview.id = "text-preview";

    // Clearing any previous response of any other tag/response type
    if (file_preview.firstElementChild)
      file_preview.innerHTML = "";

    file_preview.appendChild(preview);
  }

  preview.textContent = response_text;
}

// Displays pdf preview by creating a <object>
// Accepts response.blob()
function pdf_preview(response_blob) {
  let preview = file_preview.querySelector("object#pdf-preview");

  if (!preview) {
    preview = document.createElement("object");
    preview.id = "pdf-preview";

    // Clearing any previous response of any other tag/response type
    if (file_preview.firstElementChild)
      file_preview.innerHTML = "";

    file_preview.appendChild(preview);
  }

  preview.data = URL.createObjectURL(response_blob);
}

// Dislays a message saying that the file request is empty
function empty_preview() {
  let preview = file_preview.querySelector("div#empty-preview");

  if (!preview) {
    preview = document.createElement("div");
    preview.id = "empty-preview";

    // Clearing any previous response of any other tag/response type
    if (file_preview.firstElementChild)
      file_preview.innerHTML = "";

    file_preview.appendChild(preview);

    preview.textContent = "The Requested File is Emtpy"; // No need to edit the textContent again for this preview type
  }
}

// Displays a message saying that the file requested is not supported for previewing
// And the user should download the file instead and use it locally
function unsupported_preview() {
  let preview = file_preview.querySelector("div#unsupported-preview");

  if (!preview) {
    preview = document.createElement("div");
    preview.id = "unsupported-preview";

    // Clearing any previous response of any other tag/response type
    if (file_preview.firstElementChild)
      file_preview.innerHTML = "";

    file_preview.appendChild(preview);

    preview.textContent =
      "The Requested file is not supported for previewing.\
      Download the file by hitting 'Enter' and preview it locally on your system."; // No need to edit the textContent again for this preview type
  }
}

// Makes a request for the selected file name, if any. Fills the file preview element with the response
async function make_request() {
  if (selected_file == null)
    return;

  // Aborting any previous request that have not been resolved yet, for better performance
  if (abort_controller)
    abort_controller.abort();

  abort_controller = new AbortController();

  let fetch_url = current_url + "/" + selected_file.textContent;
  // May contain '//' if fetching directories
  fetch_url = simplify_url(fetch_url);

  const response = await fetch(fetch_url, { signal: abort_controller.signal });
  response_content_type = response.headers.get("Content-Type");
  if (response_content_type == null)
    return;

  // Handling user selecting a directory
  if (selected_file.textContent.endsWith("/"))
    dir_preview(await response.text());
  // Handling a file
  else {
    if (response_content_type.startsWith("image/")) // Handling Images
      img_preview(await response.blob());
    else if (response_content_type.startsWith("text/")) // Handling Text Files
      text_preview(await response.text());
    else if (response_content_type == "application/pdf") // Handling PDF Files
      pdf_preview(await response.blob());
    else if (response_content_type == "application/json") // Handling JSON Files
      text_preview(await response.text());
    else if (response_content_type == "application/javascript") // Handling JS Files
      text_preview(await response.text());
    else if (response_content_type.endsWith("/x-empty")) // Handling Emtpy Files
      empty_preview();
    else { // Unsupported file types
      unsupported_preview();
    }
  }
};

function select_file(to_select) {
  if (to_select == null)
    return;

  if (selected_file)
    selected_file.classList.remove("highlight");
  to_select.classList.add("highlight");
  selected_file = to_select;

  make_request();
}

// Hanldes motions and keypresses to change the highlighted file
function handle_motion() {
  document.onkeyup = (event) => {
    if (event.key == "j" || event.key == "Down" || event.key == "ArrowDown")
      select_file(selected_file.nextElementSibling);
    if (event.key == "k" || event.key == "Up" || event.key == "ArrowUp")
      select_file(selected_file.previousElementSibling);
    if (event.key == "G")
      select_file(files_list.lastElementChild);
    if (event.key == "g")
      select_file(files_list.firstElementChild);
    if (event.key == "Enter")
      visit_path(selected_file.textContent);
  }
}

document.addEventListener("DOMContentLoaded", () => {
  document.getElementById("current_url").textContent = current_path;

  files_list.id = "main-list"; // Adding id to the list for styles

  for (const file of files_list.children) {
    file.onclick = () =>
      visit_path(file.textContent);
  }

  // Highlighting the first file
  select_file(files_list.firstElementChild);

  handle_motion();
});
