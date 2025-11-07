

<h1>
<?php

$headers = apache_request_headers();

foreach ($headers as $header => $value) {
    echo "$header: $value <br />\n";
}
 echo "PHP Up and runnin'"
?>
</h1>
