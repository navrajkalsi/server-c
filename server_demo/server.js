const files_list = document.getElementsByTagName("ul")[0],
  current_url = window.location.href,
  current_path = new URL(current_url).pathname;

document.getElementById("current_url").textContent = current_path;

for (const file of files_list.children) {
  file.onclick = () => {
    window.location.href = current_url + '/' + file.textContent;
  };
}
